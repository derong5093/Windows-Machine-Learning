#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation::Collections;
using namespace Windows::AI::MachineLearning;
using namespace Windows::Media;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage;
using namespace std;

// globals
vector<string> labels;
string labelsFileName("labels.txt");
hstring modelPath;
hstring imagePath;

// helper functions
string GetModulePath();
void LoadLabels();
VideoFrame LoadImageFile(hstring filePath);
void PrintResults(IVectorView<float> results);
bool ParseArgs(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	init_apartment();

	if (ParseArgs(argc, argv) == false)
	{
		printf("Usage: %s [modelfile] [imagefile]", argv[0]);
		return -1;
	}

    std::vector <com_ptr<IDXGIAdapter1>> validAdapters = AdapterSelection::EnumerateAdapters(true);
    for (int i = 0; i < validAdapters.size(); i++) {
        DXGI_ADAPTER_DESC1 pDesc;
        com_ptr<IDXGIAdapter1> currAdapter = validAdapters.at(i);
        currAdapter->GetDesc1(&pDesc);
        printf("Index: %d, Description: %ls\n", i, pDesc.Description);
    }

    LearningModelDevice device = nullptr;
	if (validAdapters.size() == 0) {
		printf("There are no available adapters, running on CPU...\n");
        device = LearningModelDevice(LearningModelDeviceKind::Cpu);
    }
    else {
        // user selects adapter
        printf("Please enter the index of the adapter you want to use...\n");
        int selectedIndex;
        while (!(cin >> selectedIndex) || selectedIndex < 0 || selectedIndex >= validAdapters.size()) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printf("Invalid index, please try again.\n");
        }
        printf("Selected adapter at index %d\n", selectedIndex);
        device = AdapterSelection::GetLearningModelDeviceFromAdapter(validAdapters.at(selectedIndex));
    }

	// load the model
	printf("Loading modelfile '%ws' on the selected device\n", modelPath.c_str());
	DWORD ticks = GetTickCount();
	auto model = LearningModel::LoadFromFilePath(modelPath);
	ticks = GetTickCount() - ticks;
	printf("model file loaded in %d ticks\n", ticks);

	// now create a session and binding
	LearningModelSession session(model, device);
	LearningModelBinding binding(session);

	// load the image
	printf("Loading the image...\n");
	auto imageFrame = LoadImageFile(imagePath);

	// bind the input image
	printf("Binding...\n");
	binding.Bind(model.InputFeatures().GetAt(0).Name(), ImageFeatureValue::CreateFromVideoFrame(imageFrame));
	// temp: bind the output (we don't support unbound outputs yet)
	vector<int64_t> shape({ 1, 1000, 1, 1 });
	hstring outputName = model.OutputFeatures().GetAt(0).Name();
	binding.Bind(outputName, TensorFloat::Create(shape));

	// now run the model
	printf("Running the model...\n");
	ticks = GetTickCount();
	auto results = session.Evaluate(binding, L"RunId");
	ticks = GetTickCount() - ticks;
	printf("model run took %d ticks\n", ticks);

	// get the output
	auto resultTensor = results.Outputs().Lookup(outputName).as<TensorFloat>();
	auto resultVector = resultTensor.GetAsVectorView();
	PrintResults(resultVector);
}

bool ParseArgs(int argc, char* argv[])
{
	if (argc < 3)
	{
		return false;
	}
	// get the model file
	modelPath = hstring(wstring_to_utf8().from_bytes(argv[1]));
	// get the image file
	imagePath = hstring(wstring_to_utf8().from_bytes(argv[2]));
	// did they pass a fourth arg?

	return true;
}

string GetModulePath()
{
	string val;
	char modulePath[MAX_PATH] = {};
	GetModuleFileNameA(NULL, modulePath, ARRAYSIZE(modulePath));
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char filename[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath_s(modulePath, drive, _MAX_DRIVE, dir, _MAX_DIR, filename, _MAX_FNAME, ext, _MAX_EXT);

	val = drive;
	val += dir;
	return val;
}

void LoadLabels()
{
	// Parse labels from labels file.  We know the file's entries are already sorted in order.
	std::string labelsFilePath = GetModulePath() + labelsFileName;
	ifstream labelFile(labelsFilePath, ifstream::in);
	if (labelFile.fail())
	{
		printf("failed to load the %s file.  Make sure it exists in the same folder as the app\r\n", labelsFileName.c_str());
		exit(EXIT_FAILURE);
	}

	std::string s;
	while (std::getline(labelFile, s, ','))
	{
		int labelValue = atoi(s.c_str());
		if (labelValue >= labels.size())
		{
			labels.resize(labelValue + 1);
		}
		std::getline(labelFile, s);
		labels[labelValue] = s;
	}
}

VideoFrame LoadImageFile(hstring filePath)
{
	try
	{
		// open the file
		StorageFile file = StorageFile::GetFileFromPathAsync(filePath).get();
		// get a stream on it
		auto stream = file.OpenAsync(FileAccessMode::Read).get();
		// Create the decoder from the stream
		BitmapDecoder decoder = BitmapDecoder::CreateAsync(stream).get();
		// get the bitmap
		SoftwareBitmap softwareBitmap = decoder.GetSoftwareBitmapAsync().get();
		// load a videoframe from it
		VideoFrame inputImage = VideoFrame::CreateWithSoftwareBitmap(softwareBitmap);
		// all done
		return inputImage;
	}
	catch (...)
	{
		printf("failed to load the image file, make sure you are using fully qualified paths\r\n");
		exit(EXIT_FAILURE);
	}
}

void PrintResults(IVectorView<float> results)
{
    // load the labels
    LoadLabels();

    vector<pair<float, uint32_t>> sortedResults;
    for (uint32_t i = 0; i < results.Size(); i++) {
        pair<float, uint32_t> curr;
        curr.first = results.GetAt(i);
        curr.second = i;
        sortedResults.push_back(curr);
    }
    std::sort(sortedResults.begin(), sortedResults.end(),
        [](pair<float, uint32_t> const &a, pair<float, uint32_t> const &b) { return a.first > b.first; });

    // Display the result
    for (int i = 0; i < 3; i++)
    {
        pair<float, uint32_t> curr = sortedResults.at(i);
        printf("%s with confidence of %f\n", labels[curr.second].c_str(), curr.first);
    }
}

int32_t WINRT_CALL WINRT_CoIncrementMTAUsage(void** cookie) noexcept
{
	return CoIncrementMTAUsage((CO_MTA_USAGE_COOKIE*)cookie);
}
