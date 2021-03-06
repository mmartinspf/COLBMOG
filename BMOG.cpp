/*M///////////////////////////////////////////////////////////////////////////////////////
 //
 //  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 //
 //  By downloading, copying, installing or using the software you agree to this license.
 //  If you do not agree to this license, do not download, install,
 //  copy or use the software.
 //
 //
 //                          License Agreement
 //                For Open Source Computer Vision Library
 //
 // Copyright (C) 2000, Intel Corporation, all rights reserved.
 // Copyright (C) 2013, OpenCV Foundation, all rights reserved.
 // Third party copyrights are property of their respective owners.
 //
 // Redistribution and use in source and binary forms, with or without modification,
 // are permitted provided that the following conditions are met:
 //
 //   * Redistribution's of source code must retain the above copyright notice,
 //     this list of conditions and the following disclaimer.
 //
 //   * Redistribution's in binary form must reproduce the above copyright notice,
 //     this list of conditions and the following disclaimer in the documentation
 //     and/or other materials provided with the distribution.
 //
 //   * The name of the copyright holders may not be used to endorse or promote products
 //     derived from this software without specific prior written permission.
 //
 // This software is provided by the copyright holders and contributors "as is" and
 // any express or implied warranties, including, but not limited to, the implied
 // warranties of merchantability and fitness for a particular purpose are disclaimed.
 // In no event shall the Intel Corporation or contributors be liable for any direct,
 // indirect, incidental, special, exemplary, or consequential damages
 // (including, but not limited to, procurement of substitute goods or services;
 // loss of use, data, or profits; or business interruption) however caused
 // and on any theory of liability, whether in contract, strict liability,
 // or tort (including negligence or otherwise) arising in any way out of
 // the use of this software, even if advised of the possibility of such damage.
 //
 //M*/

/*//Implementation of the Gaussian mixture model background subtraction from:
 //
 //"BMOG: Boosted Gaussian Mixture Model with controlled complexity"
 //I. Martins, P. Carvalho, L. Corte-Real and J. L. Alba-Castro
 //IbPRIA 2017: 8th Iberian Conf. on Pattern Recognition and Image Analysis, proceedings to be published in Springer LNCS Series, 2017
 //
 //based upon
 //
 //"Improved adaptive Gausian mixture model for background subtraction"
 //Z.Zivkovic
 //International Conference Pattern Recognition, UK, August, 2004
 // and
 //"Efficient Adaptive Density Estimapion per Image Pixel for the Task of Background Subtraction"
 //Z.Zivkovic, F. van der Heijden
 //Pattern Recognition Letters, vol. 27, no. 7, pages 773-780, 2006.
 //
 //"Recursive unsupervised learning of finite mixture models "
 //Z.Zivkovic, F.van der Heijden
 //IEEE Trans. on Pattern Analysis and Machine Intelligence, vol.26, no.5, pages 651-656, 2004
 //
 //

 
 //Author: Isabel Martins
 //Date: 10-Jan-2017, Version:1.1
 //adapted from OpenCV MOG2 implementation
*/
 

 
#include "opencv2/video.hpp"
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"

#include "BMOG.hpp"

// C++ include files
#include <iostream>
#include <sstream>


using namespace cv;
using namespace std;


// Flags
// Background image generation method
// TRUE  = weighted average of the gaussians in the mixture
// FALSE = matched gaussian
bool bkgImage_average = true;
//bool bkgImage_average = false;


namespace cv
{
    
    /*
     Interface of BMOG from:
     
     "BMOG: Boosted Gaussian Mixture Model with controlled complexity"
     I. Martins, P. Carvalho, L. Corte-Real and J. L. Alba-Castro
     
     */
    
    // default parameters of gaussian background detection algorithm
    static const int   defaultHistory2 = 100; // Learning rate; alpha = 1/defaultHistory2 
    static const float defaultVarThreshold2_L = 35.0f;
    static const float defaultVarThreshold2_a = 12.0f;
    static const float defaultVarThreshold2_b = 12.0f;
   
    static const int   defaultNMixtures2 = 5; // maximal number of Gaussians in mixture
    static const float defaultBackgroundRatio2 = 0.9f; // threshold sum of weights for background test
    static const float defaultVarThresholdGen2 = 8.0f;
    static const float defaultVarInit2 = 11.0f; // initial variance for new components
    static const float defaultVarMax2 = 5*defaultVarInit2;
    static const float defaultVarMin2 = 4.0f;
    // additional parameters
    static const float defaultfCT2 = 0.05f; // complexity reduction prior constant 0 - no reduction of number of components
    
    // new parameters BMOG
    static const float defaultBkgLearningRate = 0.001f;
    static const float defaultUncoveredBkgLearningRate = 0.01f;
    static const float defaultFgLearningRate = 0.0005f;
    static const float defaultStepUncoveredBkgLearningRate = 0.005f;
    static const float defaultDeltaTb_L = 5.0f;
    static const float defaultDeltaTb_a = 5.0f;
    static const float defaultDeltaTb_b = 5.0f;
    
    // additional BMOG parameters
    static const int defaultPostProcMedianFilterSize = 9;   // post-processing median filter dimension
    static const int defaultPostProcMinObjSize = 0;         // post-processing minimum object size
    static const int defaultTransientFrames = 50;           // number of frames before starting hysteresis and dynamic learning rate
    static const int defaultColorSpaceConv = 0;             // 0=no_conversion  1=BGR->L*a*b*
    
    
    class BackgroundSubtractorBMOGImpl : public BackgroundSubtractorBMOG
    {
    public:
        //! the default constructor
        BackgroundSubtractorBMOGImpl()
        {
            frameSize = Size(0,0);
            frameType = 0;
            
            nframes = 0;
            history = defaultHistory2;
            varThreshold_L = defaultVarThreshold2_L;
            varThreshold_a = defaultVarThreshold2_a;
            varThreshold_b = defaultVarThreshold2_b;
            
            nmixtures = defaultNMixtures2;
            backgroundRatio = defaultBackgroundRatio2;
            fVarInit = defaultVarInit2;
            fVarMax  = defaultVarMax2;
            fVarMin = defaultVarMin2;
            
            varThresholdGen = defaultVarThresholdGen2;
            fCT = defaultfCT2;
            name_ = "BackgroundSubtractor.BMOG";
            
            BKGLearningRate = defaultBkgLearningRate;
            UNCoveredBkgLearningRate = defaultUncoveredBkgLearningRate;
            FGLearningRate = defaultFgLearningRate;
            StepUncoveredBkgLearningRate = defaultStepUncoveredBkgLearningRate;
            DeltaTb_L = defaultDeltaTb_L;
            DeltaTb_a = defaultDeltaTb_a;
            DeltaTb_b = defaultDeltaTb_b;
            
            PostProcMedianFilterSize = defaultPostProcMedianFilterSize;
            PostProcMinObjSize = defaultPostProcMinObjSize;
            transientFrames = defaultTransientFrames;
            colorSpaceConv = defaultColorSpaceConv;
        }
        
        //! the full constructor
        BackgroundSubtractorBMOGImpl(int _history,  float _varThreshold2L, float _varThreshold2ab)
        {
            frameSize = Size(0,0);
            frameType = 0;
            
            nframes = 0;
            history = _history > 0 ? _history : defaultHistory2;
            varThreshold_L = (_varThreshold2L>0)? _varThreshold2L : defaultVarThreshold2_L;
            varThreshold_a = (_varThreshold2ab>0)? _varThreshold2ab : defaultVarThreshold2_a;
            varThreshold_b = (_varThreshold2ab>0)? _varThreshold2ab : defaultVarThreshold2_b;
           
            nmixtures = defaultNMixtures2;
            backgroundRatio = defaultBackgroundRatio2;
            fVarInit = defaultVarInit2;
            fVarMax  = defaultVarMax2;
            fVarMin = defaultVarMin2;
            
            varThresholdGen = defaultVarThresholdGen2;
            fCT = defaultfCT2;
            name_ = "BackgroundSubtractor.BMOG";
            
            BKGLearningRate = defaultBkgLearningRate;
            UNCoveredBkgLearningRate = defaultUncoveredBkgLearningRate;
            FGLearningRate = defaultFgLearningRate;
            StepUncoveredBkgLearningRate = defaultStepUncoveredBkgLearningRate;
            DeltaTb_L = defaultDeltaTb_L;
            DeltaTb_a = defaultDeltaTb_a;
            DeltaTb_b = defaultDeltaTb_b;

            PostProcMedianFilterSize = defaultPostProcMedianFilterSize;
            PostProcMinObjSize = defaultPostProcMinObjSize;
            transientFrames = defaultTransientFrames;
            colorSpaceConv = defaultColorSpaceConv;

        }
        
        //! the destructor
        ~BackgroundSubtractorBMOGImpl() {}
        
        //! the update operator
        void apply(InputArray _image, OutputArray _fgmask, double learningRate);
        
        //! computes a background image which is the mean of all background gaussians
        virtual void getBackgroundImage(OutputArray backgroundImage) const;
        
        
        //! GMM re-initiaization method
        void initializeGMM(Size _frameSize, int _frameType)
        {
            frameSize = _frameSize;
            frameType = _frameType;
            nframes = 0;
            
            int nchannels = CV_MAT_CN(frameType);
            CV_Assert( nchannels <= CV_CN_MAX );
            CV_Assert( nmixtures <= 255);
            
            
            // for each gaussian mixture of each pixel bg model we store ...
            // the mixture weight (w),
            // the mean (nchannels values) and
            // the covariance for each channel
            bgmodel.create( 1, frameSize.height*frameSize.width*nmixtures*(4 + nchannels), CV_32F );
            
            //make the array for keeping track of the used modes per pixel - all zeros at start
            bgmodelUsedModes.create(frameSize,CV_8UC1);
            bgmodelUsedModes = Scalar::all(0);
            
            bgmodelPixelMode.create(frameSize,CV_8UC1);
            bgmodelPixelMode = Scalar::all(0);
            bgmodelImage.create(frameSize,CV_32FC3);
            bgmodelImage.setTo(Scalar(0.0f,0.0f,0.0f));
            
            bgmodelImage.create(frameSize,CV_32FC3);
            bgmodelImage.setTo(0.0f);
           
            //make the array for keeping track of learning rate per pixel - all zeros at start
            bgmodelLearningRate.create(frameSize,CV_32FC1);
            bgmodelLearningRate = Scalar::all(0.0);
    
            FGmaskNoPP.create(frameSize,CV_8UC1);
            FGmaskNoPP.setTo(0);

            prevFGmask.create(frameSize,CV_8UC1);
            prevFGmask.setTo(0);
            
           // std::cout << "BMOG initialized " << std::endl;
        }
        
        
        void setPreviousMask(InputArray preMask)
        {
            preMask.copyTo(prevFGmask);
            
        }

        
        void  BMOGPostProcessing(InputOutputArray _mask, int PPmedianFilterSize, int PPminObjSize)
        {
            Mat tmpMask1, tmpMask2;
            vector<vector<Point> > contours;
            vector<Vec4i> hierarchy;
            int idx;
            
            tmpMask1.create(_mask.size(), CV_8UC1);
            tmpMask1.setTo(0);
            tmpMask2.create(_mask.size(), CV_8UC1);
            tmpMask2.setTo(0);
            
            Mat mask = _mask.getMat();
            
            if (PPmedianFilterSize > 1)
                medianBlur(mask, tmpMask1, PPmedianFilterSize);
            else
                mask.copyTo(tmpMask1);
            
            //// Remove holes and all objects containing fewer than minObjectSixe pixel
            findContours(tmpMask1, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            if (contours.size() != 0)
            {
                for (idx=0; idx >= 0; idx = hierarchy[idx][0] )
                {
                    const vector<Point>& c = contours[idx];
                    double area = fabs(contourArea(Mat(c)));
                    if (area > PPminObjSize)  // (area > minObjectSize)
                    {
                        // fill contours area
                        drawContours(tmpMask2, contours, idx, 255, CV_FILLED, 8, hierarchy);
                    }
                }
            }
            
            tmpMask2.copyTo(mask);
        }
        
        
        
        virtual int getHistory() const { return history; }
        virtual void setHistory(int _nframes) { history = _nframes; }
        
        virtual int getNMixtures() const { return nmixtures; }
        virtual void setNMixtures(int nmix) { nmixtures = nmix; }
        
        virtual double getBackgroundRatio() const { return backgroundRatio; }
        virtual void setBackgroundRatio(double _backgroundRatio) { backgroundRatio = (float)_backgroundRatio; }
        
        virtual double getVarThreshold_L() const { return varThreshold_L; }
        virtual void setVarThreshold_L(double _varThreshold) { varThreshold_L = _varThreshold; }
        virtual double getVarThreshold_a() const { return varThreshold_a; }
        virtual void setVarThreshold_a(double _varThreshold) { varThreshold_a = _varThreshold; }
        virtual double getVarThreshold_b() const { return varThreshold_b; }
        virtual void setVarThreshold_b(double _varThreshold) { varThreshold_b = _varThreshold; }
        
        virtual double getVarThresholdGen() const { return varThresholdGen; }
        virtual void setVarThresholdGen(double _varThresholdGen) { varThresholdGen = (float)_varThresholdGen; }
        
        virtual double getVarInit() const { return fVarInit; }
        virtual void setVarInit(double varInit) { fVarInit = (float)varInit; }
        
        virtual double getVarMin() const { return fVarMin; }
        virtual void setVarMin(double varMin) { fVarMin = (float)varMin; }
        
        virtual double getVarMax() const { return fVarMax; }
        virtual void setVarMax(double varMax) { fVarMax = (float)varMax; }
        
        virtual double getComplexityReductionThreshold() const { return fCT; }
        virtual void setComplexityReductionThreshold(double ct) { fCT = (float)ct; }
        
        virtual double getBkgLearningRate() const { return BKGLearningRate; }
        virtual double getUncoveredBkgLearningRate() const { return UNCoveredBkgLearningRate; }
        virtual double getFgLearningRate() const { return FGLearningRate; }
        virtual double getStepUncoveredBkgLearningRate() const { return StepUncoveredBkgLearningRate; }
        
        virtual void setBkgLearningRate(double learningRate) {BKGLearningRate = learningRate; }
        virtual void setUncoveredBkgLearningRate(double learningRate) {UNCoveredBkgLearningRate = learningRate; }
        virtual void setFgLearningRate(double learningRate) {FGLearningRate = learningRate; }
        virtual void setStepUncoveredBkgLearningRate(double stepLearningRate) {StepUncoveredBkgLearningRate = stepLearningRate; }
        
        virtual double getDeltaTb_L() const { return DeltaTb_L; }
        virtual double getDeltaTb_a() const { return DeltaTb_a; }
        virtual double getDeltaTb_b() const { return DeltaTb_b; }
        virtual void   setDeltaTb_L(double delta) {DeltaTb_L = delta; }
        virtual void   setDeltaTb_a(double delta) {DeltaTb_a = delta; }
        virtual void   setDeltaTb_b(double delta) {DeltaTb_b = delta; }
                
        virtual int getPostProcessingMedianFilterSize() const { return PostProcMedianFilterSize; }
        virtual void setPostProcessingMedianFilterSize(int value) {PostProcMedianFilterSize = value; }
 
        virtual int getPostProcessingMinObjectSize() const { return PostProcMinObjSize; }
        virtual void setPostProcessingMinObjectSize(int value) {PostProcMinObjSize = value; }

        virtual int getTransientFrames() const { return transientFrames; }
        virtual void setTransientFrames(int value) { transientFrames = value; }
        
        //! selection of color space: 0->no_conversion   1->L*a*b*  1->YUV  2->YCbCr
        virtual int getColorSpaceConversion() const  { return colorSpaceConv; }
        virtual void setColorSpaceConversion(int value) { colorSpaceConv = value; }
        
        
        virtual void write(FileStorage& fs) const
        {
            fs << "name" << name_
            << "history" << history
            << "nmixtures" << nmixtures
            << "backgroundRatio" << backgroundRatio
            << "varThreshold _L" << varThreshold_L
            << "varThreshold _a" << varThreshold_a
            << "varThreshold _b" << varThreshold_b
            << "varThresholdGen" << varThresholdGen
            << "varInit" << fVarInit
            << "varMin" << fVarMin
            << "varMax" << fVarMax
            << "complexityReductionThreshold" << fCT;
        }
        
        virtual void read(const FileNode& fn)
        {
            CV_Assert( (String)fn["name"] == name_ );
            history = (int)fn["history"];
            nmixtures = (int)fn["nmixtures"];
            backgroundRatio = (float)fn["backgroundRatio"];
            varThreshold_L = (double)fn["varThreshold_L"];
            varThreshold_a = (double)fn["varThreshold_a"];
            varThreshold_b = (double)fn["varThreshold_b"];
            varThresholdGen = (float)fn["varThresholdGen"];
            fVarInit = (float)fn["varInit"];
            fVarMin = (float)fn["varMin"];
            fVarMax = (float)fn["varMax"];
            fCT = (float)fn["complexityReductionThreshold"];
        }
        

    protected:
        Size frameSize;
        int  frameType;
        
        Mat bgmodel;
        Mat bgmodelUsedModes;    //keep track of number of modes per pixel
        Mat bgmodelPixelMode;    //keep track of most probable mode per pixel
        Mat bgmodelImage;
        
        Mat bgmodelLearningRate; //keep track of learning rate
        Mat prevFGmask;          //keep previous FG/BG mask
        Mat FGmaskNoPP;          //keep FG/BG mask wihout Post Processing

        int nframes;
        int history;
        int nmixtures;
        //! here it is the maximum allowed number of mixture components.
        //! Actual number is determined dynamically per pixel
        double varThreshold_L;
        double varThreshold_a;
        double varThreshold_b;
        // threshold on the squared Mahalanobis distance to decide if it is well described
        // by the background model or not. Related to Cthr from the paper.
        // This does not influence the update of the background. A typical value could be 4 sigma
        // and that is varThreshold=4*4=16; Corresponds to Tb in the paper.
        
        /////////////////////////
        // less important parameters - things you might change but be carefull
        ////////////////////////
        float backgroundRatio;
        // corresponds to fTB=1-cf from the paper
        // TB - threshold when the component becomes significant enough to be included into
        // the background model. It is the TB=1-cf from the paper. So I use cf=0.1 => TB=0.9
        // For alpha=0.001 it means that the mode should exist for approximately 105 frames before
        // it is considered foreground
        // float noiseSigma;
        float varThresholdGen;
        //correspondts to Tg - threshold on the squared Mahalan. dist. to decide
        //when a sample is close to the existing components. If it is not close
        //to any a new component will be generated. I use 3 sigma => Tg=3*3=9.
        //Smaller Tg leads to more generated components and higher Tg might make
        //lead to small number of components but they can grow too large
        float fVarInit;
        float fVarMin;
        float fVarMax;
        //initial variance  for the newly generated components.
        //It will will influence the speed of adaptation. A good guess should be made.
        //A simple way is to estimate the typical standard deviation from the images.
        //I used here 10 as a reasonable value
        // min and max can be used to further control the variance
        float fCT;//CT - complexity reduction prior
        //this is related to the number of samples needed to accept that a component
        //actually exists. We use CT=0.05 of all the samples.
        
        String name_;
        
        float BKGLearningRate;
        float UNCoveredBkgLearningRate;
        float FGLearningRate;
        float StepUncoveredBkgLearningRate;
        float DeltaTb_L, DeltaTb_a, DeltaTb_b;
        
        int PostProcMedianFilterSize;
        int PostProcMinObjSize;
        
        int transientFrames;
        int colorSpaceConv;

    };
    
    struct GaussBGStatModel2Params
    {
        //image info
        int nWidth;
        int nHeight;
        int nND;//number of data dimensions (image channels)
        
        double  minArea; // for postfiltering
        
        bool bInit;     //default 1, faster updates at start
        
        /////////////////////////
        //very important parameters - things you will change
        ////////////////////////
        float fAlphaT;
        //alpha - speed of update - if the time interval you want to average over is T
        //set alpha=1/T. It is also usefull at start to make T slowly increase
        //from 1 until the desired T
        float fTb_L;
        float fTb_a;
        float fTb_b;
        //Tb - threshold on each color component distance to decide if it is well described
        //by the background model or not.
        //This does not influence the update of the background. Value for Tb_L is typically higher than Tb_a, Tb_b;
        // typically Tb_b=Tb_a; Tb_L=3*Tb_a;
        
        /////////////////////////
        //less important parameters - things you might change but be carefull
        ////////////////////////
        float fTg;
        //Tg - threshold on the squared Mahalan. dist. to decide
        //when a sample is close to the existing components. If it is not close
        //to any a new component will be generated. I use 3 sigma => Tg=3*3=9.
        //Smaller Tg leads to more generated components and higher Tg might make
        //lead to small number of components but they can grow too large
        float fTB;//1-cf from the paper
        //TB - threshold when the component becomes significant enough to be included into
        //the background model. It is the TB=1-cf from the paper. So I use cf=0.1 => TB=0.
        //For alpha=0.001 it means that the mode should exist for approximately 105 frames before
        //it is considered foreground
        float fVarInit;
        float fVarMax;
        float fVarMin;
        //initial standard deviation  for the newly generated components.
        //It will will influence the speed of adaptation. A good guess should be made.
        //A simple way is to estimate the typical standard deviation from the images.
        //I used here 10 as a reasonable value
        float fCT;//CT - complexity reduction prior
        //this is related to the number of samples needed to accept that a component
        //actually exists. We use CT=0.05 of all the samples. By setting CT=0 you get
        //the standard Stauffer&Grimson algorithm (maybe not exact but very similar)
        
        //even less important parameters
        int nM;//max number of modes
        
    };
    
    struct GMM
    {
        float weight;
        float variance[3];
    };
    
    
    //update GMM - the base update function performed per pixel
    //
    // "BMOG: Boosted Gaussian Mixture Model with controlled complexity"
    // I. Martins, P. Carvalho, L. Corte-Real and J. L. Alba-Castro
    //
    
    class BMOGInvoker : public ParallelLoopBody
    {
    public:
        
        BMOGInvoker(const Mat& _src,
                    const Mat& _prevFgMask, Mat& _dst,
                    GMM* _gmm, float* _mean,
                    uchar* _modesUsed, /*uchar* _modeMatch,*/ Mat& _bkgImage,
                    int _nmixtures, Mat& _learningRate,
                    float _Tb_L, float _Tb_a, float _Tb_b,
                    float _TB, float _Tg,
                    float _varInit, float _varMin, float _varMax,
                    float _CT,
                    float _bkgLearningRate,
                    float _uncoveredBkgLearningRate,
                    float _fgLearningRate,
                    float _stepUncoveredBkgLearningRate,
                    float _deltaTb_L, float _deltaTb_a, float _deltaTb_b,
                    bool _transient)
        {
            src = &_src;
            prevFgMask0 = &_prevFgMask;
            dst = &_dst;
            gmm0 = _gmm;
            mean0 = _mean;
            modesUsed0 = _modesUsed;
            bkgImage0 = &_bkgImage;
            nmixtures = _nmixtures;
            learningRate0 = &_learningRate;
            Tb_L = _Tb_L;
            Tb_a = _Tb_a;
            Tb_b = _Tb_b;
            deltaTb_L = _deltaTb_L;
            deltaTb_a = _deltaTb_a;
            deltaTb_b = _deltaTb_b;
            TB = _TB;
            Tg = _Tg;
            varInit = _varInit;
            varMin = MIN(_varMin, _varMax);
            varMax = MAX(_varMin, _varMax);
            CT = _CT;  // CT
            bkgLearningRate = _bkgLearningRate,
            uncoveredBkgLearningRate = _uncoveredBkgLearningRate,
            fgLearningRate = _fgLearningRate,
            stepUncoveredBkgLearningRate = _stepUncoveredBkgLearningRate,
            transient = _transient;
        }
        
        
        
        void operator()(const Range& range) const
        {
            int y0 = range.start, y1 = range.end;
            int ncols = src->cols, nchannels = src->channels();
            AutoBuffer<float> buf(src->cols*nchannels);
            float dData[CV_CN_MAX];
           
            
            for( int y = y0; y < y1; y++ )
            {
                const float* data = buf;
                if( src->depth() != CV_32F )
                    src->row(y).convertTo(Mat(1, ncols, CV_32FC(nchannels), (void*)data), CV_32F);
                else
                    data = src->ptr<float>(y);
                
                
                float* mean = mean0 + ncols*nmixtures*nchannels*y;
                GMM* gmm = gmm0 + ncols*nmixtures*y;
                uchar* modesUsed = modesUsed0 + ncols*y;
                uchar* mask = dst->ptr(y);
                
                float* bkgImgPt = bkgImage0->ptr<float>(y);
               
                float* alphaPt = learningRate0->ptr<float>(y);
                
                const uchar* prevFgMaskPt; // MOGdelay
                prevFgMaskPt = prevFgMask0->ptr<uchar>(y);
               

                for( int x = 0; x < ncols; x++, data += nchannels, gmm += nmixtures, mean += nmixtures*nchannels )
                {
                    //calculate distances to the modes (+ sort)
                    //here we need to go in descending order!!!
                    bool background = false;//return value -> true - the pixel classified as background
                    
                    //internal:
                    bool fitsPDF = false;//if it remains zero a new GMM mode will be added
                    
                    int nmodes = modesUsed[x]; //current number of modes in GMM
                    float totalWeight = 0.f;
                    
                    float* mean_m = mean;
                    
                    
                    uchar prevFgPixel = (uchar)0;
                    if ( prevFgMaskPt[x] == (uchar)255 ) prevFgPixel = (uchar)255;

                    bkgImgPt += nchannels;
                    vector<float> pixel(nchannels, 0.f);
                    // get last BKG img pixel value
                    pixel[0] = bkgImgPt[0];
                    pixel[1] = bkgImgPt[1];
                    pixel[2] = bkgImgPt[2];
                   
                    float alphaT = alphaPt[x]; // current pixel learning rate
                    
                    float prune = -alphaT*CT;
                    float alpha1 = 1.f - alphaT;
                    
                    
                    float newTb_L =  Tb_L;
                    float newTb_a =  Tb_a;
                    float newTb_b =  Tb_b;
                    // Hysteresis: set new thresholds
                    if (!transient )
                    {
                        if (prevFgPixel == (uchar)255)
                        {
                            newTb_L -= deltaTb_L;
                            newTb_a -= deltaTb_a;
                            newTb_b -= deltaTb_b;
                        }
                        else
                        {
                            newTb_L += deltaTb_L;
                            newTb_a += deltaTb_a;
                            newTb_b += deltaTb_b;
                        }
                    }
                    
                    //////
                    //go through all modes
                    for( int mode = 0; mode < nmodes; mode++, mean_m += nchannels )
                    {
                        // eq. (14)  weight = (1-alpha)*weight - alpha*CT
                        float weight = alpha1*gmm[mode].weight + prune; //need only weight if fit is found
                        int swap_count = 0;
                        ////
                        

                        //fit not found yet
                        if( !fitsPDF )
                        {
                            //check if it belongs to some of the remaining modes
                            float var[3];
                            var[0] = gmm[mode].variance[0];
                            var[1] = gmm[mode].variance[1];
                            var[2] = gmm[mode].variance[2];
                           
                            //calculate difference and distance
                            if( nchannels == 3 )
                            {
                                dData[0] = mean_m[0] - data[0];
                                dData[1] = mean_m[1] - data[1];
                                dData[2] = mean_m[2] - data[2];
                            }
                            else
                            {
                                for( int c = 0; c < nchannels; c++ )
                                {
                                    dData[c] = mean_m[c] - data[c];
                                }
                            }
                            
                            
                            if ( (totalWeight < TB) && (dData[0]*dData[0] < newTb_L*var[0]) && (dData[1]*dData[1] < newTb_a*var[1]) && (dData[2]*dData[2] < newTb_b*var[2]))
                                background = true;
                            
                            /////////// Dynamic Learning Rate start ///////////
                            if (!transient)
                            {
                                if ( background )
                                {
                                    if ( prevFgPixel == (uchar)255) // Uncovered Background
                                        alphaT = uncoveredBkgLearningRate ;
                                    else
                                    {
                                        alphaT -= stepUncoveredBkgLearningRate;
                                        alphaT = (alphaT > bkgLearningRate*nmodes) ?  alphaT : bkgLearningRate*nmodes ;
                                    }
                                }
                                else
                                    alphaT = fgLearningRate;
                            }
                            /////////// Dynamic Learning Rate end ///////////

                            
                            alpha1 = 1.f - alphaT;
                            prune = -alphaT*CT;
                            
                            // eq. (14)  weight = (1-alpha)*weight - alpha*CT
                            weight = alpha1*gmm[mode].weight + prune;//need only weight if fit is found
                            
                            //check fit "close" component
                            if ( (dData[0]*dData[0] < Tg*var[0]) && (dData[1]*dData[1] < Tg*var[1]) && (dData[2]*dData[2] < Tg*var[2]) )
                            {
                                /////
                                //belongs to the mode
                                fitsPDF = true;
                                
                                //update distribution
                                
                                //update weight
                                // eq. (14)
                                weight += alphaT;
                                float k = alphaT/weight;
                                
                                //update mean
                                //eq. (5)
                                for( int c = 0; c < nchannels; c++ )
                                    mean_m[c] -= k*dData[c];
                                
                                //update variances
                                // eq. (6)
                                float varnew;
                                for( int c = 0; c < nchannels; c++ )
                                {
                                    varnew = var[c] + k*(dData[c]*dData[c]-var[c]);
                                    //limit the variance update speed
                                    varnew = MAX(varnew, varMin);
                                    varnew = MIN(varnew, varMax);
                                    gmm[mode].variance[c] = varnew;
                                }
                               
                                // set new BKG pixel value
                                if (background)
                                {
                                    for( int c = 0; c < nchannels; c++ )
                                        pixel[c] = mean_m[c];
                                }
                                
                                //sort
                                //all other weights are at the same place and
                                //only the matched (iModes) is higher -> just find the new place for it
                                for( int i = mode; i > 0; i-- )
                                {
                                    //check one up
                                    if( weight < gmm[i-1].weight )
                                        break;
                                    
                                    swap_count++;
                                    //swap one up
                                    std::swap(gmm[i], gmm[i-1]);
                                    for( int c = 0; c < nchannels; c++ )
                                        std::swap(mean[i*nchannels + c], mean[(i-1)*nchannels + c]);
                                    
                                }
                                //belongs to the mode - bFitsPDF becomes 1
                                /////
                            }
                        }//!FitsPDF)
                        
                        //check prune
                        if( weight < -prune )
                        {
                            weight = 0.0;
                            nmodes--;
                        }
                        
                        gmm[mode-swap_count].weight = weight;  //update weight by the calculated value
                        totalWeight += weight;
                        
                    }
                    //go through all modes
                    //////
                    
                    //renormalize weights
                    totalWeight = 1.f/totalWeight;
                    for( int mode = 0; mode < nmodes; mode++ )
                    {
                        gmm[mode].weight *= totalWeight;
                    }
                    
                    
                    //make new mode if needed and exit
                    if( !fitsPDF && alphaT > 0.f )
                    {
                        
                        // replace the weakest or add a new one
                        int mode = nmodes == nmixtures ? nmixtures-1 : nmodes++;
                        
                        if (nmodes==1)
                            gmm[mode].weight = 1.f;
                        else
                        {
                            gmm[mode].weight = alphaT;
                            
                            // renormalize all other weights
                            for( int i = 0; i < nmodes-1; i++ )
                                gmm[i].weight *= alpha1;
                        }
                        
                        // init
                        for( int c = 0; c < nchannels; c++ )
                            mean[mode*nchannels + c] = data[c];
                        
                        for( int c = 0; c < nchannels; c++ )
                           gmm[mode].variance[c] = varInit;
                        
                        // set new BKG pixel value
                        /*
                        if (background)
                         {
                         for( int c = 0; c < nchannels; c++ )
                            pixel[c] = mean_m[c];
                         }
                        */
                        
                        
                        //sort
                        //find the new place for it
                        for( int i = nmodes - 1; i > 0; i-- )
                        {
                            // check one up
                            if( alphaT < gmm[i-1].weight )
                                break;
                            
                            // swap one up
                            std::swap(gmm[i], gmm[i-1]);
                            for( int c = 0; c < nchannels; c++ )
                                std::swap(mean[i*nchannels + c], mean[(i-1)*nchannels + c]);
                        }
                        
                     } // if( !fitsPDF && alphaT > 0.f )
                    

                    
                    // updated pixel learning rate
                    alphaPt[x] = alphaT;

                    // set the number of modes
                    modesUsed[x] = uchar(nmodes);
                    
                    // set BKG img pixel
                    for( int c = 0; c < nchannels; c++ )
                        bkgImgPt[c] = pixel[c];
                    
                    // set pixel classification
                    mask[x] = background ? 0 : 255;
             
                 }
            }
        }
        
        
        const Mat* src;
        const Mat* prevFgMask0;
        Mat* dst;
        GMM* gmm0;
        float* mean0;
        uchar* modesUsed0;
        //uchar* modeMatch0;
        Mat* bkgImage0;
        Mat* learningRate0;
        int nmixtures;
        float alphaT, Tb_L, Tb_a, Tb_b, TB, Tg;
        float varInit, varMin, varMax, CT, tau;
        
        float deltaTb_L;
        float deltaTb_a;
        float deltaTb_b;
        
        float bkgLearningRate;
        float uncoveredBkgLearningRate;
        float fgLearningRate;
        float stepUncoveredBkgLearningRate;
        
        bool  transient;
        
    };
    
    

    
    void BackgroundSubtractorBMOGImpl::apply(InputArray _image, OutputArray _fgmask, double learningRate)
    {
        Mat Lab_image;
        
        bool needToInitialize = nframes == 0 || learningRate >= 1 || _image.size() != frameSize || _image.type() != frameType;
        
        if( needToInitialize )
            initializeGMM(_image.size(), _image.type());
        
        
        // convert input frame from BGR to L*a*b / YUV / YCbCr  color space
        Lab_image.create(_image.size(), _image.type());
        switch (colorSpaceConv)
        {
            case 0:
                // no conversion
                _image.copyTo(Lab_image);
                break;
            case 1:
                cvtColor(_image, Lab_image, CV_BGR2Lab);
                break;
            case 2:
                cvtColor(_image, Lab_image, CV_BGR2YUV);
                break;
            case 3:
                cvtColor(_image, Lab_image, CV_BGR2YCrCb);
                break;
            default:
                // no conversion
                _image.copyTo(Lab_image);
        }
        
        
        _fgmask.create( _image.size(), CV_8UC1 );
        Mat fgmask = _fgmask.getMat();
        
        
        bool isTransient = false;
        
        ++nframes;
        if (nframes < transientFrames)
        {
            if (PostProcMedianFilterSize == 0) // AngleBMOG
                learningRate = learningRate >= 0 && nframes > 1 ? learningRate : 1./std::min( 2*nframes, history ); // ok AngleMOG
            else
                learningRate = 1./history; // ColorBMOG

            CV_Assert(learningRate >= 0);
            bgmodelLearningRate.setTo(learningRate);
            isTransient = true;
        }
        
        parallel_for_(Range(0, Lab_image.rows),
                      BMOGInvoker(Lab_image,
                                  prevFGmask, fgmask,
                                  bgmodel.ptr<GMM>(),
                                  (float*)(bgmodel.ptr() + sizeof(GMM)*nmixtures*Lab_image.rows*Lab_image.cols),
                                  bgmodelUsedModes.ptr(), bgmodelImage,
                                  nmixtures, bgmodelLearningRate,
                                  (float)varThreshold_L, (float)varThreshold_a, (float)varThreshold_b,
                                  backgroundRatio, varThresholdGen,
                                  fVarInit, fVarMin, fVarMax, fCT,
                                  (float) BKGLearningRate,
                                  (float) UNCoveredBkgLearningRate,
                                  (float) FGLearningRate,
                                  (float) StepUncoveredBkgLearningRate,
                                  (float) DeltaTb_L, (float) DeltaTb_a, (float) DeltaTb_b,
                                  isTransient),
                      Lab_image.total()/(double)(1 << 16));
        
        fgmask.copyTo(FGmaskNoPP);

        if ((PostProcMedianFilterSize != 0) || (PostProcMinObjSize != 0))
            BMOGPostProcessing(fgmask, PostProcMedianFilterSize, PostProcMinObjSize);
        
        fgmask.copyTo(prevFGmask);
        
        
        /*
         Mat showModes;
         Mat showLearningRates;

         bgmodelLearningRate.convertTo(showLearningRates, CV_8UC1, 51000, 0);
         bgmodelUsedModes.convertTo(showModes, CV_8UC1, 50, 0);
         namedWindow("window BMOG.2", CV_WINDOW_NORMAL);
         cvMoveWindow("window BMOG.2", 25+2*frameSize.width, frameSize.height+40);
         imshow("window BMOG.2", showLearningRates);
        
        cout << "fim apply" << endl;
        waitKey(1000);
        */
    }
    
    
    
    void BackgroundSubtractorBMOGImpl::getBackgroundImage(OutputArray backgroundImage)  const
    {
        if (bkgImage_average)
        {
            // background image is weighted mean of gaussians in the mixture
            int nchannels = CV_MAT_CN(frameType);
            CV_Assert(nchannels == 1 || nchannels == 3);
            Mat meanBackground(frameSize, CV_MAKETYPE(CV_8U, nchannels), Scalar::all(0));
            int firstGaussianIdx = 0;
            const GMM* gmm = bgmodel.ptr<GMM>();
            const float* mean = reinterpret_cast<const float*>(gmm + frameSize.width*frameSize.height*nmixtures);
            std::vector<float> meanVal(nchannels, 0.f);
            for(int row=0; row<meanBackground.rows; row++)
            {
                for(int col=0; col<meanBackground.cols; col++)
                {
                    int nmodes = bgmodelUsedModes.at<uchar>(row, col);
                    float totalWeight = 0.f;
                    for(int gaussianIdx = firstGaussianIdx; gaussianIdx < firstGaussianIdx + nmodes; gaussianIdx++)
                    {
                        GMM gaussian = gmm[gaussianIdx];
                        size_t meanPosition = gaussianIdx*nchannels;
                        for(int chn = 0; chn < nchannels; chn++)
                        {
                            meanVal[chn] += gaussian.weight * mean[meanPosition + chn];
                        }
                        totalWeight += gaussian.weight;
                    
                        // eq. (8)
                        if(totalWeight > backgroundRatio)
                            break;
                    }
                    float invWeight = 1.f/totalWeight;
                    switch(nchannels)
                    {
                        case 1:
                            meanBackground.at<uchar>(row, col) = (uchar)(meanVal[0] * invWeight);
                            meanVal[0] = 0.f;
                            break;
                        case 3:
                            Vec3f& meanVec = *reinterpret_cast<Vec3f*>(&meanVal[0]);
                            meanBackground.at<Vec3b>(row, col) = Vec3b(meanVec * invWeight);
                            meanVec = 0.f;
                            break;
                    }
                    firstGaussianIdx += nmixtures;
                }
            }
            meanBackground.copyTo(backgroundImage);
        }
        else
        {
            // background image pixel is the mean of the matched gaussian in the mixture
            bgmodelImage.convertTo(backgroundImage, CV_8UC3);
        }
    }
    
    
    
    Ptr<BackgroundSubtractorBMOG> createBackgroundSubtractorBMOG(int _history, double _varThresholdL, double _varThresholdab)
    {
        return makePtr<BackgroundSubtractorBMOGImpl>(_history, (float)_varThresholdL, (float)_varThresholdab);
    }
    
}

/* End of file. */

