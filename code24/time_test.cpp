// test frame 83 84


#include <stdio.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <map>
#include "opencv2/core/core.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/nonfree/nonfree.hpp"

using namespace std;
using namespace cv;




void drawMatch(Mat& img1,
                Mat& img2,
                Mat& img_matches,
                Ptr<SurfFeatureDetector> &detector,
                int max_hessian = 800,
                int min_hessian = 25,
                int max_size = 400,
                int min_size = 200, 
                double ratio = 0.8,
                bool extended = false){
    
                    
    vector<KeyPoint>key_points_img1, key_points_img2;
    detector->detect(img1, key_points_img1);
    detector->detect(img2, key_points_img2);

    /*
    int hessian = detector->hessianThreshold;
    cout << "Threshold_start:" << detector->hessianThreshold <<endl;
    while ((key_points_img1.size() < min_size || key_points_img1.size() > min_size) && hessian >= min_hessian & hessian <= max_hessian){
        if ( key_points_img1.size() < min_size ){
            hessian = (int)hessian/1.5;
            detector.release();
            detector = new SurfFeatureDetector(hessian);
            detector->detect(img1, key_points_img1);
            detector->detect(img2, key_points_img2);
        }
        else
        {
            hessian = (int)hessian/1.5;
            detector.release();
            detector = new SurfFeatureDetector(hessian);
            detector->detect(img1, key_points_img1);
            detector->detect(img2, key_points_img2);
            
        }
        
    }
    
    cout << "Threshold_end:" << detector->hessianThreshold <<endl;
    */

    if(key_points_img1.size() < 4 || key_points_img2.size() < 4){
        return ;
    }

    Mat descriptor_img1, descriptor_img2;
    detector->compute(img1, key_points_img1, descriptor_img1);
    detector->compute(img2, key_points_img2, descriptor_img2);
    
    
    BFMatcher matcher;
    vector<vector<DMatch>>matches;
    matcher.knnMatch(descriptor_img1, descriptor_img2, matches, 2);
    
    vector<DMatch> good_matches;

    if(descriptor_img1.empty() || descriptor_img2.empty()){
        return ;
    }
    map<int, DMatch> matches_map;
   
   

    for(int i = 0; i < matches.size(); i++)
    {
        if (matches[i][0].distance < ratio * matches[i][1].distance)
        {
            if(matches_map.count(matches[i][0].trainIdx) == 0){
                matches_map[matches[i][0].trainIdx] = matches[i][0];
            }
            else{
                if (matches_map[matches[i][0].trainIdx].distance > matches[i][0].distance){
                    matches_map[matches[i][0].trainIdx] = matches[i][0];
                }
            }
        }
    }
      

    for (map<int, DMatch>::iterator it = matches_map.begin(); it != matches_map.end(); it++){
        good_matches.push_back(it->second);
    }

    if (good_matches.size() < 4){
        return ;
    }

    
    sort(good_matches.begin(), good_matches.end());


    
    vector<Point2f> img1_points;
    vector<Point2f> img2_points;
    
    int num_good = min((int)good_matches.size(), (int)good_matches.size());
    vector<DMatch> first_matches;
    
    for(unsigned int i = 0; i < num_good; ++i)
    {
        img1_points.push_back(key_points_img1[good_matches[i].queryIdx].pt);
        img2_points.push_back(key_points_img2[good_matches[i].trainIdx].pt);
        first_matches.push_back(good_matches[i]);
    }

    

   

    drawMatches(img1, key_points_img1, img2, key_points_img2, first_matches, img_matches);
    
    cv::putText(img_matches, to_string(key_points_img1.size()) + "   " + to_string(key_points_img2.size()), Point(0, 50), FONT_HERSHEY_COMPLEX, 1, Scalar(0, 0, 0) );
    Mat H = findHomography(img1_points, img2_points, RANSAC, 5);
    
    vector<Point2f> img1_cross(5);
    Point2f middle = Point2f(img1.cols/2, img1.rows/2);
    img1_cross[0]=middle+Point2f(0,-50);
    img1_cross[1]=middle+Point2f(0, 50);
    img1_cross[2]=middle+Point2f(-50, 0);
    img1_cross[3]=middle+Point2f(50, 0);
    img1_cross[4]=middle;
    vector<Point2f> img2_cross(5);
   
    if (!H.empty()){
        
        perspectiveTransform(img1_cross, img2_cross, H);

        line(img_matches, img1_cross[0], img1_cross[1], Scalar(0, 255, 0), 2);
        line(img_matches, img1_cross[2], img1_cross[3], Scalar(0, 255, 0), 2);
        
        line(img_matches, img2_cross[0]+Point2f(img1.cols, 0), img2_cross[1]+Point2f(img1.cols, 0), Scalar(0, 255, 0), 2);
        line(img_matches, img2_cross[2]+Point2f(img1.cols, 0), img2_cross[3]+Point2f(img1.cols, 0), Scalar(0, 255, 0), 2);
        circle(img_matches, img2_cross[4]+Point2f(img1.cols, 0), 5, Scalar(0, 0, 255), 2);
    }

    return ;
  
}


void drawMatch1(Mat& img1, Mat& img2, Mat& img_matches, double ratio = 0.75, bool extended = false){
   
    int minHessian=400;
    SurfFeatureDetector  detector(minHessian, 4, 3, false);
    vector<KeyPoint>key_points_img1, key_points_img2;
    detector.detect(img1, key_points_img1);
    detector.detect(img2, key_points_img2);
    /*
    while (key_points_img1.size()<100 && minHessian >= 25){
        minHessian /= 2;
        SurfFeatureDetector  detector(minHessian, 4, 3, false);
        detector.detect(img1, key_points_img1);
        detector.detect(img2, key_points_img2);
    }
    */
    
    if(key_points_img1.size() < 4 || key_points_img2.size() < 4){
        return ;
    }

    Mat descriptor_img1, descriptor_img2;
    detector.compute(img1, key_points_img1, descriptor_img1);
    detector.compute(img2, key_points_img2, descriptor_img2);
    
    
    BFMatcher matcher;
    vector<vector<DMatch>>matches;
    matcher.knnMatch(descriptor_img1, descriptor_img2, matches, 2);
    
    vector<DMatch> good_matches;

    if(descriptor_img1.empty() || descriptor_img2.empty()){
        return ;
    }
    map<int, DMatch> matches_map;
   
    if (matches.size() < 30){
        ratio = 0.9;
    }

    for(int i = 0; i < matches.size(); i++)
    {
        if (matches[i][0].distance < ratio * matches[i][1].distance)
        {
            if(matches_map.count(matches[i][0].trainIdx) == 0){
                matches_map[matches[i][0].trainIdx] = matches[i][0];
            }
            else{
                if (matches_map[matches[i][0].trainIdx].distance > matches[i][0].distance){
                    matches_map[matches[i][0].trainIdx] = matches[i][0];
                }
            }
        }
    }
      

    for (map<int, DMatch>::iterator it = matches_map.begin(); it != matches_map.end(); it++){
        good_matches.push_back(it->second);
    }

    if (good_matches.size() < 4){
        return ;
    }

    
    sort(good_matches.begin(), good_matches.end());


    
    vector<Point2f> img1_points;
    vector<Point2f> img2_points;
    
    int num_good = min((int)good_matches.size(),50);
    vector<DMatch> first_matches;
    
    for(unsigned int i = 0; i < num_good; ++i)
    {
        img1_points.push_back(key_points_img1[good_matches[i].queryIdx].pt);
        img2_points.push_back(key_points_img2[good_matches[i].trainIdx].pt);
        first_matches.push_back(good_matches[i]);
    }

    

   

    drawMatches(img1, key_points_img1, img2, key_points_img2, first_matches, img_matches);
    
    cv::putText(img_matches, to_string(key_points_img1.size()) + "   " + to_string(key_points_img2.size()), Point(0, 50), FONT_HERSHEY_COMPLEX, 1, Scalar(0, 0, 0) );
    Mat H = findHomography(img1_points, img2_points, RANSAC, 1);
    
    vector<Point2f> img1_cross(5);
    Point2f middle = Point2f(img1.cols/2, img1.rows/2);
    img1_cross[0]=middle+Point2f(0,-50);
    img1_cross[1]=middle+Point2f(0, 50);
    img1_cross[2]=middle+Point2f(-50, 0);
    img1_cross[3]=middle+Point2f(50, 0);
    img1_cross[4]=middle;
    vector<Point2f> img2_cross(5);
   
    if (!H.empty()){
        
        perspectiveTransform(img1_cross, img2_cross, H);

        line(img_matches, img1_cross[0], img1_cross[1], Scalar(0, 255, 0), 2);
        line(img_matches, img1_cross[2], img1_cross[3], Scalar(0, 255, 0), 2);
        
        line(img_matches, img2_cross[0]+Point2f(img1.cols, 0), img2_cross[1]+Point2f(img1.cols, 0), Scalar(0, 255, 0), 2);
        line(img_matches, img2_cross[2]+Point2f(img1.cols, 0), img2_cross[3]+Point2f(img1.cols, 0), Scalar(0, 255, 0), 2);
        circle(img_matches, img2_cross[4]+Point2f(img1.cols, 0), 5, Scalar(0, 0, 255), 2);
    }
       
    return ;
  
}

int main(int argc, char const *argv[])
{
    string comp_folder = "/home/jiaming/opencv24/code24/out_frames/";
    string folder = "/home/jiaming/opencv24/code24/img";
    string img1_name = "1/";
    string img2_name = "2/";
    string extention = ".png";
    int i=0;
    Mat out, out2;
   
    Ptr<SurfFeatureDetector> detector = new SurfFeatureDetector(400);
    //while (i<115)
    //{

    Mat img1 = imread(folder + img1_name + to_string(i) + extention, IMREAD_COLOR);
    Mat img2 = imread(folder + img2_name + to_string(i) + extention, IMREAD_COLOR);
    
    time_t start, end;
    //Mat org = imread(comp_folder + to_string(i) + extention, IMREAD_COLOR);
    start = clock();
    drawMatch(img1, img2, out, detector);
    end = clock();
    cout<<"Time for pointer: "<< (end - start) * 1.0 / CLOCKS_PER_SEC <<endl;

    start = clock();
    drawMatch1(img1, img2, out2);
    end = clock();
    cout<<"Time for object: "<< (end - start) * 1.0 / CLOCKS_PER_SEC <<endl;

    //if (!out.empty())
    //    imshow("Out", out);
    //waitKey(0);
    
    //drawMatch(img2, img1, reverse);
    //if (!reverse.empty())
        //imshow("Reverse", reverse);
    
    //i++;
    //
    //}
    
   

    return 0;
}

