#include<memory>
#include <opencv2/opencv.hpp>
#include<opencv2/dnn/dnn.hpp>
#include<algorithm>
#include<vector>
#include<string>
#include<sstream>
#include<iomanip>
#include<cstdio>
#include<iostream>

#ifdef _WIN32
#define POPEN  _popen
#define PCLOSE _pclose
#else
#define POPEN  popen
#define PCLOSE pclose
#endif

struct goal
{
    cv::Rect position{ 0,0,0,0 }; float confident{ 0.0 }; std::string id{ " " };
};
class FfmpegPipeWriter
{
public:
    ~FfmpegPipeWriter() { close(); }

    bool open(const std::string& ffmpegExe, const std::string& outFile,
              const std::string& audioSource, int width, int height, double fps,
              int crf = 20, int audioBitrateK = 192)
    {
        this->w = width; this->h = height;
        std::ostringstream cmd;
        cmd << '"' << ffmpegExe << "\" -hide_banner -loglevel error -y"
            << " -f rawvideo -pix_fmt bgr24 -video_size " << width << 'x' << height
            << " -framerate " << std::fixed << std::setprecision(6) << fps
            << " -i -"                                      
            << " -i \"" << audioSource << "\""             
            << " -map 0:v:0 -map 1:a:0?"                      
            << " -c:v libx264 -preset veryfast -crf " << crf << " -pix_fmt yuv420p"
            << " -c:a aac -b:a " << audioBitrateK << "k"
            << " -shortest -movflags +faststart"
            << " \"" << outFile << "\"";
        std::string full = "\"" + cmd.str() + "\"";
        pipe = POPEN(full.c_str(), "wb");
        return pipe != nullptr;
    }

    void write(const cv::Mat& frame)
    {
        if (!pipe || frame.empty()) return;
        cv::Mat bgr = frame;
        if (bgr.type() != CV_8UC3) cv::cvtColor(bgr, bgr, cv::COLOR_GRAY2BGR);
        if (bgr.cols != w || bgr.rows != h) cv::resize(bgr, bgr, cv::Size(w, h));
        if (!bgr.isContinuous()) bgr = bgr.clone();
        std::fwrite(bgr.data, 1, (size_t)bgr.total() * bgr.elemSize(), pipe);
    }

    void close()
    {
        if (!pipe) return;
        PCLOSE(pipe);     
        pipe = nullptr;
    }

    bool is_open() const { return pipe != nullptr; }

private:
    FILE* pipe{ nullptr };
    int   w{ 0 }, h{ 0 };
};

static bool ffmpegAvailable(const std::string& exe)
{
    std::string cmd = "\"\"" + exe + "\" -version 2>&1\"";   
    FILE* p = POPEN(cmd.c_str(), "r");
    if (!p) return false;
    char buf[256] = { 0 };
    bool ok = std::fgets(buf, sizeof(buf), p) != nullptr &&
              std::string(buf).find("ffmpeg version") != std::string::npos;
    PCLOSE(p);
    return ok;
}

std::unique_ptr<std::vector<goal>> detect(const cv::Mat&M,cv::dnn::Net&net)
{
    int len = std::max(M.cols,M.rows);
    cv::Mat sq(cv::Size(len,len),CV_8UC3,cv::Scalar(114,114,114));
    M.copyTo(sq(cv::Rect((sq.cols-M.cols)/2,(sq.rows-M.rows)/2,M.cols,M.rows)));
    cv::Mat blob = cv::dnn::blobFromImage(sq,1.0/255.0,cv::Size(640,640),cv::Scalar(0,0,0),true,false);
    net.setInput(blob); cv::Mat out = net.forward();
    cv::Mat data = out.reshape(1,out.size[1]);
    std::vector<cv::Rect>boxes; std::vector<float>confidents; std::vector<int>index;
    for (int i = 0; i < data.rows; ++i)
    {

        float* p = data.ptr<float>(i);
        int left = ((p[0]-(p[2]/2)) * ((double)len / 640.0)) - ((double)len / 2) + ((double)M.cols / 2);
        int top = ((p[1]-(p[3]/2)) * ((double)len / 640.0)) - ((double)len / 2) + ((double)M.rows/2);
        int width = p[2] * ((double)len / 640.0); int height = p[3] * ((double)len / 640.0);
        int id = 5;
        left = std::min(std::max(0, left), M.cols - width); top = std::min(std::max(0, top), M.rows - height);
        if (p[4]*p[id] < 0.25)continue;
        else
        {
            boxes.push_back(cv::Rect(left,top,width,height)); confidents.push_back(p[4]);
            index.push_back(id-5);
        }
    }
    std::vector<int>indices;
    cv::dnn::NMSBoxes(boxes,confidents,0.25f,0.45f,indices);
    std::unique_ptr<std::vector<goal>>p=std::make_unique<std::vector<goal>>(indices.size());
    for (int u=0;u<indices.size();++u)
    {
        auto pp = p->begin() + u;
        pp->position = boxes[indices[u]]; pp->confident = confidents[indices[u]]; pp->id = [](int index) {
            switch (index)
            {
            case 0:return "face"; break;
            default:return "fail"; break;
            }

            }(index[indices[u]]);
    }
    return p;
}

int main()
{
    const std::string srcVideo  = "assert/row.mp4";    
    const std::string outFile   = "output.mp4";  
    const std::string ffmpegExe = "ffmpeg";     
                                                

    cv::VideoCapture cup(srcVideo);
    if (!cup.isOpened()) { std::cerr << "open failed: " << srcVideo << "\n"; return -1; }
    double fps = cup.get(cv::CAP_PROP_FPS);
    if (fps < 1.0 || fps > 240.0) fps = 30.0;
    int W = (int)cup.get(cv::CAP_PROP_FRAME_WIDTH);
    int H = (int)cup.get(cv::CAP_PROP_FRAME_HEIGHT);
    W &= ~1; H &= ~1;   

    cv::Mat img = cv::imread("input.jpg");
    if (img.empty()) { std::cerr << "read failed: input.jpg\n"; return -1; }
    cv::dnn::Net net = cv::dnn::readNetFromONNX("Flandre.onnx");

    FfmpegPipeWriter pipe;
    cv::VideoWriter   fallback;
    bool usePipe = ffmpegAvailable(ffmpegExe) &&
                   pipe.open(ffmpegExe, outFile, srcVideo, W, H, fps);

    if (usePipe)
    {
        std::cout << "[info] writing " << outFile << " (video+audio, " << W << 'x' << H
                  << " @ " << std::fixed << std::setprecision(3) << fps << "fps)\n";
    }
    else
    {
      
        fallback.open("ouput.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, cv::Size(W, H));
        if (!fallback.isOpened()) { std::cerr << "VideoWriter open failed\n"; return -1; }
        std::cout << "[warn] ffmpeg not found, writing silent output.avi.\n"
                  << "       afterwards run:\n"
                  << "       ffmpeg -i output.avi -i " << srcVideo
                  << " -map 0:v:0 -map 1:a:0 -c:v libx264 -crf 20 -preset veryfast"
                  << " -c:a aac -b:a 192k -shortest output.mp4\n";
    }

    cv::Mat M; long long n = 0;
    while (cup.read(M))
    {
        if (M.cols != W || M.rows != H) M = M(cv::Rect(0, 0, W, H));

        auto outs = detect(M, net);
        if (outs->size() != 0) {
            for (auto& out : *outs) {
                if (out.position.width <= 0 || out.position.height <= 0) continue;
                cv::Mat std_img;
                cv::resize(img, std_img, cv::Size(out.position.width, out.position.height));
                std_img.copyTo(M(out.position));
            }
        }

        if (usePipe) pipe.write(M); else fallback.write(M);
        ++n;
    }

    pipe.close();                
    if (fallback.isOpened()) fallback.release();
    std::cout << "[info] done, " << n << " frames -> " << (usePipe ? outFile : std::string("output.avi")) << "\n";
    return 0;
}
