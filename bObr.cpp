#include <string>
#include <iostream>
#include <sstream>
#include <exception>
#include <libCZI/libCZI.h>
#include <vips/vips8>
#include <experimental/filesystem>

using namespace std;
using namespace vips;
using namespace std::experimental::filesystem;

const int MAXINT = numeric_limits<int>::max();


void convert2vips(const vector<shared_ptr<libCZI::ISubBlock>> &subblocks, int min_x, int min_y, int width, int height, const vector<double> &colour, char *location){
    VImage black = VImage::black(width, height, vips::VImage::option() -> set("bands",3));
    VImage white = black.new_from_image(colour);
    white.vipssave(location);
    VImage loaded = VImage::vipsload(location);
    
    for (auto &sub: subblocks){
        auto info = sub->GetSubBlockInfo();
	int w = info.physicalSize.w;
    	int h = info.physicalSize.h;
        int pos_x = info.logicalRect.x - min_x;
        int pos_y = info.logicalRect.y - min_y;
        
	auto bitmap = sub->CreateBitmap();
        libCZI::IBitmapData* bm = bitmap.get();
        libCZI::ScopedBitmapLocker<libCZI::IBitmapData*> lckBm{ bm };
        auto bitmap_data = lckBm.ptrDataRoi;
	size_t size = lckBm.size;
        VImage dataimage = VImage::new_from_memory(bitmap_data, size, w, h, 3, VipsBandFormat::VIPS_FORMAT_UCHAR);
	vector<vips::VImage> bands = dataimage.bandsplit();
        VImage switch_bgr = bands[0];
        bands[0] = bands[2];
 	bands[2] = switch_bgr;
 	VImage out = VImage::bandjoin(bands);
	loaded.draw_image(out, pos_x, pos_y);
    }
    cout << "Conversion done for " << location << endl;
}

/// overloaded operator () for comapring subblocks
//  firstly by S (in case of more scenes in one image), secondly by M

struct mIndexComparator
{
    bool operator() (const shared_ptr<libCZI::ISubBlock>& struct1, const shared_ptr<libCZI::ISubBlock>& struct2)
    {
        const libCZI::SubBlockInfo& info1 = struct1->GetSubBlockInfo();
        const libCZI::SubBlockInfo& info2 = struct2->GetSubBlockInfo();
	int sub1S, sub2S = 0;
        info1.coordinate.EnumValidDimensions([&sub1S](libCZI::DimensionIndex dim, int start)->bool{
	    if (dim == libCZI::DimensionIndex::S){
	        sub1S = start;
	    }
	    return true;
	});
	info2.coordinate.EnumValidDimensions([&sub2S](libCZI::DimensionIndex dim2, int start2)->bool{
            if (dim2 == libCZI::DimensionIndex::S) {
       	        sub2S = start2;
	    }
	    return true;
	});
	return ((sub1S < sub2S) || ((sub1S == sub2S) && (info1.mIndex < info2.mIndex)));
    }
};


vector<double> getBackgroundColour(shared_ptr<libCZI::ISubBlock>& subblock){
    auto bitmap = subblock->CreateBitmap();
    libCZI::IBitmapData* bm = bitmap.get();
    libCZI::ScopedBitmapLocker<libCZI::IBitmapData*> lckBm{ bm };
    auto bitmap_data = lckBm.ptrDataRoi;
    const uint8_t* px = static_cast<const uint8_t*>(bitmap_data);
    return vector<double> {double(px[2]), double(px[1]), double(px[0])}; 
}


void one(const shared_ptr<libCZI::ICZIReader> &cziReader, int plane, int width, int height, const string& location, bool one){
    int min_x, min_y = MAXINT;
    vector<shared_ptr<libCZI::ISubBlock>> subblocks;
    const string loc = location + to_string(plane) + ".v";
    auto func = [&plane, cziReader, &subblocks, &min_x, &min_y, &one](int index, const libCZI::SubBlockInfo& info)
    {
        if ((info.mIndex != MAXINT) && (info.mIndex >= 0)){
                info.coordinate.EnumValidDimensions([&plane, &index, cziReader, &subblocks, &info, &min_x, &min_y, &one](libCZI::DimensionIndex dim, int start)->bool{
                    if (one) {
                       auto subblock = cziReader->ReadSubBlock(index);
		       if (info.logicalRect.x < min_x){
                          min_x = info.logicalRect.x;
                       }
                       if (info.logicalRect.y < min_y){
                           min_y = info.logicalRect.y;
                       }
                       subblocks.emplace_back(subblock);
                    } else {
                        if ((dim == libCZI::DimensionIndex::Z) && (start == plane)) {
                            auto subblock = cziReader->ReadSubBlock(index);
                            if (info.logicalRect.x < min_x){
                                min_x = info.logicalRect.x;
                            }
                            if (info.logicalRect.y < min_y){
                                min_y = info.logicalRect.y;
                            }
                            subblocks.emplace_back(subblock);
                       }
                    }
                    return true;
                });
        }
        return true;
    };
    
    cziReader->EnumerateSubBlocks(func);
    sort(subblocks.begin(), subblocks.end(), mIndexComparator());
    
    /// conversion from string to char* as type of parameter 'location' in draw_image(VIPS) is char*
    // https://stackoverflow.com/questions/7352099/stdstring-to-char
    
    const vector<double> col = getBackgroundColour(subblocks[0]);
    vector<char> cstr(loc.c_str(), loc.c_str() + loc.size() + 1);
    convert2vips(subblocks, min_x, min_y, width, height, col, &cstr[0]);
}


void multiple(const shared_ptr<libCZI::ICZIReader> &cziReader, int planes, int width, int height, const string& location){
    for (int i = 0; i < planes; i++){
        one(cziReader, i, width, height, location, false);
    }
}


int main(int argc, char** argv) {
    
    if ((argc < 2) or (argc > 3)){
        cerr << "Usage: " << argv[0] << " [full path to .czi file]  [optional - full path where to save]" << endl;
        return 1;
    }

    /// check if VIPS library is started

    if (VIPS_INIT (argv[0]))
        vips_error_exit (NULL);
    
    try{
	
	/// I need to modify input string but argv is const, therefore copying it to new variable  
	
	string path_to_file = argv[1];
        size_t found = path_to_file.find_last_of('/');
	string filename =  path_to_file.substr(found+1);
	
	/// convert string to char*, required in function libCZI::CreateStreamFromFile
        
	const size_t sz = strlen(argv[1]) + 1;
	unique_ptr<wchar_t[]> text{new wchar_t[sz]};
        mbstowcs(text.get(), argv[1], sz);

	/// optional path where to save the result, otherwise cwd
	
	string location = "";
        if (argc == 3){
	    location = argv[2];
	    if (location.back() != '/') {
	        location += '/';
	    }
	    if (!exists(location)){
	        cerr << "Location " << location << " does not exist!" << endl;
		return 1;
	    }
        } else {
	    string cwd = current_path();
	    location = cwd + '/';
        }
	location += filename;

	cout << "Opening CZI file" << endl;
        auto stream = libCZI::CreateStreamFromFile(text.get());
        auto cziReader = libCZI::CreateCZIReader();
        cziReader->Open(stream);
	
	/// determinig width, height and the number of planes in .czi file
	
	auto statistics = cziReader->GetStatistics();
    	int width = statistics.boundingBox.w;
	int height =  statistics.boundingBox.h;
        int planes = 0;

        statistics.dimBounds.EnumValidDimensions([&planes](libCZI::DimensionIndex dim, int start, int size)->bool
                {
                    if (dim == libCZI::DimensionIndex::Z) {
                        planes = size;
                    }
                    return true;
                });
	
	if (planes == 0){
	    one(cziReader, 0, width, height, location, true);
	} else {
            multiple(cziReader, planes, width, height, location);
	}

    } catch (const exception & ex) {
        cout << endl << ex.what() << endl;
    }

    return 0;
}
