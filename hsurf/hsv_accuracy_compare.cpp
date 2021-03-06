#include <opencv2/opencv.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <numeric>
using namespace cv;
using namespace cv::xfeatures2d;
using namespace std;



Mat get_mask(Mat img, Point2f center, int r){
    Mat mask = Mat::zeros(img.size(), CV_8UC1);
    circle(mask, center, r, Scalar(255), -1);
    return mask;
}



double* align_and_normalize_hist(double * hist, int length){
    int max_index = 0;
    double total = 0;
    for (int i=1; i<length; i++){
        if(hist[i] > hist[i-1]){
            max_index = i;
        }
        total += hist[i];
    }
    double* out = new double[length]();
    int index = 0;
    for(int i=max_index; i<length; i++){
        out[index] = hist[i] / total;
        index++;
    }
    for(int i=0; i<max_index; i++){
        out[index] = hist[i] / total;
        index++;
    }
    delete [] hist;
    return out;
}


double* get_hist(Mat img_hsv, Point center, int r = 10, int bins = 60){
    
    Mat g_kernal_x = getGaussianKernel(2*r, 2);
    Mat g_kernal_y = getGaussianKernel(2*r, 2);
    Mat g_kernal_t = g_kernal_x * g_kernal_y.t();
    Mat g_kernal = g_kernal_t(Rect(r, r, r, r));
    normalize(g_kernal, g_kernal, 1, 100, NORM_MINMAX);
    g_kernal.convertTo(g_kernal, CV_8UC1);
    //cout<<g_kernal<<endl;
    double span = 360 / bins;
    double* temp = new double[bins]();
    
    //cout<<format(g_kernal, Formatter::FMT_PYTHON)<<endl;
    //cout<<img_hsv.size<<endl;
    int i_start = max((int)center.y - r, 0);
    int i_end = min((int)center.y + r, (int)img_hsv.rows);
    int j_start = max((int)center.x - r, 0);
    int j_end = min((int)center.x + r, (int)img_hsv.cols);
    for (int i = i_start; i < i_end; i++){
        for (int j = j_start; j < j_end; j++){
            int dy = abs(i - center.y);
            int dx = abs(j - center.x);
            if ( dx*dx + dy*dy > r*r)
                continue;
            //cout<<(int)img_hsv.at<uchar>(i, j)<<endl;
            
            int h = img_hsv.at<uchar>(i, j);
            //cout<<h<<endl;
            //cout<<(int)g_kernal.at<uchar>(dx, dy)<<endl;
            temp[(int)(h / span)] += (int)g_kernal.at<uchar>(dy, dx);
        }
    }
    
    temp = align_and_normalize_hist(temp, bins);
    
    return temp;
}



double b_distance(double* hist1, double* hist2, int bins){
    double s = 0;
    for (int i=0; i<bins; i++){
        s += sqrt(hist1[i] * hist2[i]);
    }
    delete[] hist1;
    delete[] hist2;
    return s;
}


double get_distance(Mat img1_h, Mat img2_h, KeyPoint kp1, KeyPoint kp2, int bins = 60){
   
    
    double* h1 = get_hist(img1_h, kp1.pt, kp1.size, bins);
    double* h2 = get_hist(img2_h, kp2.pt, kp2.size, bins);

    return b_distance(h1, h2, bins);
}




Mat addPad(Mat img){
    int length = max(img.cols, img.rows);
    Mat shiftImg = Mat::zeros(2 * length, 2 * length, img.type());
    Mat ROI = shiftImg(Rect(length - img.cols / 2, length - img.rows / 2, img.cols, img.rows));
    img.copyTo(ROI);
    return shiftImg;
}

Point2f getPosition(Point2f p, Mat affineMat){
    double x = p.x * affineMat.at<double>(0, 0) + p.y * affineMat.at<double>(0, 1) + affineMat.at<double>(0, 2);
    double y = p.x * affineMat.at<double>(1, 0) + p.y * affineMat.at<double>(1, 1) + affineMat.at<double>(1, 2);
    return Point2f(x, y);
}


Mat rotateImg(Mat img, Mat affineMat, Point2f center){
    
    
    Mat out;
    //cout<<format(affineMat, Formatter::FMT_PYTHON)<<endl;
    warpAffine(img, out, affineMat, img.size() , INTER_LINEAR, BORDER_TRANSPARENT); 
    return out;
    
}

bool isCorrect(Point2f p1, Point2f p2, double d = 10){
    return d > sqrt((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y));
}

pair<Mat, double> draw_match(Mat img1, Mat img2, Mat affineMat, int angle, double scale, int matchNum = 50, bool extended = true){
   
    int minHessian=400;
    Ptr<SURF>  detector = SURF::create(minHessian, 4, 3, extended);
    vector<KeyPoint>key_points_img1, key_points_img2;
    detector->detect(img1, key_points_img1);
    detector->detect(img2, key_points_img2);
 
    
    Mat descriptor_img1, descriptor_img2;
    detector->compute(img1, key_points_img1, descriptor_img1);
    detector->compute(img2, key_points_img2, descriptor_img2);
    
    
    BFMatcher matcher;
    vector<vector<DMatch>>matches;
    matcher.knnMatch(descriptor_img1, descriptor_img2, matches, 2);
    
    vector<DMatch> goodMatches;

    double ratio = 0.8;

    for(int i = 0; i < matches.size(); i++)
    {
        
        if (matches[i][0].distance < ratio * matches[i][1].distance)
        {
            goodMatches.push_back(matches[i][0]);
        }

    }
    
    sort(goodMatches.begin(), goodMatches.end());

    Mat img_matches = Mat::zeros(img1.rows, 2 * img1.cols, img1.type());
    Mat ROI = img_matches(Rect(0, 0, img1.cols, img1.cols));
    img1.copyTo(ROI);
    ROI = img_matches(Rect(img1.cols, 0, img1.cols, img1.rows));
    img2.copyTo(ROI);

    Point2f p1, p2, position;
    Point2f shift = Point2f(img1.cols, 0);

    int good = 0;

    for(unsigned int i = 0; i < min(matchNum, (int)goodMatches.size()); ++i)
    {
        p1 = key_points_img1[goodMatches[i].queryIdx].pt;
        p2 = key_points_img2[goodMatches[i].trainIdx].pt;
        position = getPosition(p1, affineMat);
        if (isCorrect(p2, position))
        {
            line(img_matches, p1, p2+shift, Scalar(0, 255, 0));
            good++;
        }
            
        else
        {
            line(img_matches, p1, p2+shift, Scalar(0, 0, 255));
        }
            

    }

    double accuracy = good * 1.0 / matchNum;
    string ac = "Accuracy: ";
    string ag = "Angle: ";
    string sc = "Scale: ";
    cout<< "Normal Accuracy:" << accuracy <<endl;
    putText(img_matches, ac + to_string(accuracy) + " " + ag + to_string(angle) + " " + sc + to_string(scale), Point(100, 100), HersheyFonts::FONT_HERSHEY_PLAIN, 2, Scalar(0, 255, 0));
    pair<Mat, double> out = pair<Mat, double>(img_matches, accuracy);

   return out;
}

pair<Mat, double> draw_match_hsv(Mat img1, Mat img2, Mat affineMat, int angle, double scale, double weight, int matchNum = 50, bool extended = true){
   
    int minHessian=400;
    Ptr<SURF>  detector = SURF::create(minHessian, 4, 3, extended);
    vector<KeyPoint>key_points_img1, key_points_img2;
    detector->detect(img1, key_points_img1);
    detector->detect(img2, key_points_img2);
 
    
    Mat descriptor_img1, descriptor_img2;
    detector->compute(img1, key_points_img1, descriptor_img1);
    detector->compute(img2, key_points_img2, descriptor_img2);
    
    
    BFMatcher matcher;
    vector<vector<DMatch>>matches;
    matcher.knnMatch(descriptor_img1, descriptor_img2, matches, 2);
    
    vector<DMatch> goodMatches;

    double ratio = 0.8;

    for(int i = 0; i < matches.size(); i++)
    {
        
        if (matches[i][0].distance < ratio * matches[i][1].distance)
        {
            int query_index = matches[i][0].queryIdx;
            int train_index = matches[i][0].trainIdx;
            double hsv_distance = get_distance(img1, img2, key_points_img1[query_index], key_points_img2[train_index]) ;
            //cout<<hsv_distance<<endl;
            double t = matches[i][0].distance;
            matches[i][0].distance = (1 - weight) * t + weight * (1 - hsv_distance) * t;
            goodMatches.push_back(matches[i][0]);
        }

    }
    
    sort(goodMatches.begin(), goodMatches.end());

    
   
 
    Mat img_matches = Mat::zeros(img1.rows, 2 * img1.cols, img1.type());
    Mat ROI = img_matches(Rect(0, 0, img1.cols, img1.cols));
    img1.copyTo(ROI);
    ROI = img_matches(Rect(img1.cols, 0, img1.cols, img1.rows));
    img2.copyTo(ROI);

    Point2f p1, p2, position;
    Point2f shift = Point2f(img1.cols, 0);

    int good = 0;

    for(unsigned int i = 0; i < min(matchNum, (int)goodMatches.size()); ++i)
    {
        p1 = key_points_img1[goodMatches[i].queryIdx].pt;
        p2 = key_points_img2[goodMatches[i].trainIdx].pt;
        position = getPosition(p1, affineMat);
        if (isCorrect(p2, position))
        {
            line(img_matches, p1, p2+shift, Scalar(0, 255, 0));
            good++;
        }
            
        else
        {
            line(img_matches, p1, p2+shift, Scalar(0, 0, 255));
        }
            

    }

    double accuracy = good * 1.0 / matchNum;
    //cout<<accuracy<<endl;
    string ac = "Accuracy: ";
    string ag = "Angle: ";
    string sc = "Scale: ";
    cout<< "HSV Accuracy: " << accuracy <<endl;
    putText(img_matches, ac + to_string(accuracy) + " " + ag + to_string(angle) + " " + sc + to_string(scale), Point(100, 100), HersheyFonts::FONT_HERSHEY_PLAIN, 2, Scalar(0, 255, 0));
    pair<Mat, double> out = pair<Mat, double>(img_matches, accuracy);

   return out;
}

//Usage: <Img> <Angle> <Scale> <MatchNum>

int main(int argc, char const *argv[])
{
    if (argc != 6){
        cout<<"Usage: <Img> <Step> <Scale> <MatchNum> <Weight>"<<endl;
        exit(0);
    }

    string folder = "/home/jiaming/Documents/research/img/";
    string img1_name = argv[1];
    int step= strtol(argv[2], NULL, 10);
    double scale = strtod(argv[3], NULL);
    int matchNum = strtol(argv[4], NULL, 10);
    double weight = strtod(argv[5], NULL);
    Mat img1 = imread(folder + img1_name, IMREAD_COLOR);
    Mat img1_hsv;
    cvtColor(img1, img1_hsv, COLOR_BGR2HSV_FULL); 
  
    Mat pad = addPad(img1);
    Point2f center = Point2f(pad.cols / 2, pad.rows / 2);
    vector<double> ac_normal, ac_hsv;
    for(int i =0 ; i <= 360 ; i += step){
        cout<<"Angle: "<<to_string(i)<<endl;
        Mat affineMat = getRotationMatrix2D(center, i, scale);
        Mat rot = rotateImg(pad, affineMat, center);
        
        Mat pad_hsv = addPad(img1_hsv);
        pair<Mat, double> img_ac;
        //Mat affineMat = getRotationMatrix2D(center, angle, scale);
        Mat rot_hsv = rotateImg(pad_hsv, affineMat, center);
        img_ac = draw_match(pad, rot, affineMat, i, scale, matchNum);
        ac_normal.push_back(img_ac.second);
        //imshow("Regular", img_ac.first);
        img_ac = draw_match_hsv(pad_hsv, rot_hsv, affineMat, i, scale, weight, matchNum);
        ac_hsv.push_back(img_ac.second);
        //imshow("HSV", img_ac.second);
        //waitKey(1000);
    }
    cout<<"Average accuracy for normal surf: "<< accumulate(ac_normal.begin(), ac_normal.end(), 0.0) / ac_normal.size()<<endl;
    cout<<"Average accuracy for hsv surf: "<< accumulate(ac_hsv.begin(), ac_hsv.end(), 0.0) / ac_hsv.size()<<endl;
    

    
    //waitKey(0);
    return 0;
}

