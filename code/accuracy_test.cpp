#include <opencv2/opencv.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <numeric> //accumulate
#include <fstream>
#include <cmath>
#include <climits> //INT_MAX
#include <opencv2/core/persistence.hpp>
using namespace cv;
using namespace cv::xfeatures2d;
using namespace std;



Mat get_mask(Mat img, Point2f center, int r){
    Mat mask = Mat::zeros(img.size(), CV_8UC1);
    circle(mask, center, r, Scalar(255), -1);
    
    return mask;
}

Mat get_hist(Mat img_hvs, Point2f center, int r = 10, int bins = 60){
    Mat mask = get_mask(img_hvs, center, r);
    int hist_size[] = {bins};
    float hranges[] = {0, 180};
    const float* ranges[] = {hranges};
    int chanels [] = {0};
    MatND hist;
    calcHist(&img_hvs, 1, chanels, mask, hist, 1, hist_size, ranges);
    Scalar totoal = sum(hist);
    hist = hist * 1.0 / totoal[0];
    //
    //Scalar s = sum(hist);
    //cout<<"s:"<<s[0]<<endl;
    resize(hist, hist, Size(bins,1));
    //cout<<format(hist, Formatter::FMT_PYTHON)<<endl;
    return hist;
}




Point2f getPosition(Point2f p, Mat affineMat){
    double x = p.x * affineMat.at<double>(0, 0) + p.y * affineMat.at<double>(0, 1) + affineMat.at<double>(0, 2);
    double y = p.x * affineMat.at<double>(1, 0) + p.y * affineMat.at<double>(1, 1) + affineMat.at<double>(1, 2);
    return Point2f(x, y);
}



bool isCorrect(Point2f p1, Point2f p2, double d = 10){
    return d > sqrt((p1.x - p2.x)*(p1.x - p2.x) + (p1.y - p2.y)*(p1.y - p2.y));
}


vector<vector<DMatch>> my_knn_matcher(Mat img1, Mat img2, Mat des1, Mat des2, vector<KeyPoint> kp1, vector<KeyPoint> kp2, double weight){
    Mat hue_img1;
    Mat hue_img2;
    cvtColor(img1, img1, COLOR_BGR2HSV_FULL);
    cvtColor(img2, img2, COLOR_BGR2HSV_FULL);
    for (int i=0; i<kp1.size(); i++){
        hue_img1.push_back(get_hist(img1, kp1[i].pt, kp1[i].size));
    }
    for (int i=0; i<kp2.size(); i++){
        hue_img2.push_back(get_hist(img2, kp2[i].pt, kp2[i].size));
    }
    //cout<<format(hue_img1.row(0), Formatter::FMT_PYTHON)<<endl;
    //cout<<format(hue_img2.row(0), Formatter::FMT_PYTHON)<<endl;
    
    vector<vector<DMatch>> knn_matches;
    for (int i=0; i<kp1.size(); i++){
        vector<DMatch> d_match(2);
        DMatch d1;
        DMatch d2;
        d1.distance = INT_MAX * 1.0 -1;
        d2.distance = INT_MAX * 1.0;
        d1.queryIdx = i;
        d2.queryIdx = i;
        for (int j=0; j<kp2.size(); j++){
            double norm_distance = norm(des1.row(i), des2.row(j), NORM_L2);
            double hue_distance = hue_img1.row(i).dot(hue_img2.row(j));
            //cout<<format(hue_img1.row(i), Formatter::FMT_PYTHON)<<endl;
            //cout<<format(hue_img2.row(j), Formatter::FMT_PYTHON)<<endl;
            double weighted_distance = (1 - weight) * norm_distance - (weight * norm_distance * hue_distance);
            if (weighted_distance < d1.distance){
                d1.distance = weighted_distance;
                d1.trainIdx = j;
            }
            else if (weighted_distance < d2.distance){
                d2.distance = weighted_distance;
                d2.trainIdx = j;
            }
        }
        d_match[0] = d1;
        d_match[1] = d2;
        knn_matches.push_back(d_match);
    }

    return knn_matches;
}


Mat draw_match(Mat img1, Mat img2, Mat affineMat, int matchNum = 50, bool extended = false){
   
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
    Mat img_matches = Mat::zeros(img1.rows, img1.cols + img2.cols, img1.type());
    Mat ROI = img_matches(Rect(0, 0, img1.cols, img1.rows));
    img1.copyTo(ROI);
    ROI = img_matches(Rect(img1.cols, 0, img2.cols, img2.rows));
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

    double accuracy = good * 1.0 / min(matchNum, (int)goodMatches.size());
    string ac = "Accuracy: ";
    cout<< "Normal Accuracy:" << accuracy <<endl;
    putText(img_matches, ac + to_string(accuracy), Point(50, 50), HersheyFonts::FONT_HERSHEY_PLAIN, 2, Scalar(0, 0, 255));
    pair<Mat, double> out = pair<Mat, double>(img_matches, accuracy);

   return img_matches;
}

Mat draw_match_hsv(Mat img1, Mat img2, Mat affineMat, double weight, int matchNum = 50, bool extended = false){
   
    int minHessian=400;
    Ptr<SURF>  detector = SURF::create(minHessian, 4, 3, extended);
    vector<KeyPoint>key_points_img1, key_points_img2;
    detector->detect(img1, key_points_img1);
    detector->detect(img2, key_points_img2);
 
    
    Mat descriptor_img1, descriptor_img2;
    detector->compute(img1, key_points_img1, descriptor_img1);
    detector->compute(img2, key_points_img2, descriptor_img2);
    
    
    vector<vector<DMatch>>matches;
    matches = my_knn_matcher(img1, img2, descriptor_img1, descriptor_img2, key_points_img1, key_points_img2, weight);
    
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

    
   
 
    Mat img_matches = Mat::zeros(img1.rows, img1.cols + img2.cols, img1.type());
    Mat ROI = img_matches(Rect(0, 0, img1.cols, img1.rows));
    img1.copyTo(ROI);
    ROI = img_matches(Rect(img1.cols, 0, img2.cols, img2.rows));
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

    double accuracy = good * 1.0 / min(matchNum, (int)goodMatches.size());
    //cout<<accuracy<<endl;
    string ac = "Accuracy: ";
    
    cout<< "HSV Accuracy: " << accuracy <<endl;
    putText(img_matches, ac + to_string(accuracy), Point(50, 50), HersheyFonts::FONT_HERSHEY_PLAIN, 2, Scalar(0, 0, 255));
    pair<Mat, double> out = pair<Mat, double>(img_matches, accuracy);

   return img_matches;
}

Mat read_h(string file_name){
    Mat out = Mat::zeros(2, 3, CV_64FC1);
    ifstream in(file_name);
    double n;
    for (int i = 0; i<2 ; i++){
        for (int j = 0; j < 3; j++){
            in >> n;
            out.at<double>(i, j) = n;
        }
    }
    //cout<<format(out, Formatter::FMT_PYTHON)<<endl;
    return out;

}
//Usage: <ImgFoder> <ImgNum1> <ImgNum2> <Weight> <MatchNum>

int main(int argc, char const *argv[])
{
    if (argc != 6){
        cout<<"Usage: <ImgFoder> <ImgNum1> <ImgNum2> <Weight> <MatchNum>"<<endl;
        exit(0);
    }

    string base_folder = "/home/jiaming/Documents/research/dataset/";
    string img_folder = argv[1];
    string img_num1 = argv[2];
    string img_num2 = argv[3];
    double weight = strtod(argv[4], NULL);
    int matchNum = strtol(argv[5], NULL, 10);

    Mat img1 = imread(base_folder + img_folder + "/img_70" + img_num1 + ".ppm", IMREAD_COLOR);
    Mat img2 = imread(base_folder + img_folder + "/img_70" + img_num2 + ".ppm", IMREAD_COLOR);
    imshow("img1", img1);
    waitKey(0);
    imshow("img2", img2);
    waitKey(0);
    Mat affine_matrix = read_h(base_folder+img_folder+"/H"+img_num1+"to"+img_num2);
    cout<<base_folder+img_folder+"/H"+img_num1+"to"+img_num2<<endl;
    cout<<affine_matrix<<endl;
    Mat out_normal = draw_match(img1, img2, affine_matrix, matchNum);
    Mat out_hsv = draw_match_hsv(img1, img2, affine_matrix, weight, matchNum);
    
    imshow("Normal", out_normal);
    imshow("HSV", out_hsv);
    waitKey(0);
    return 0;
}

