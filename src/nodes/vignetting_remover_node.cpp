/// Copyright 2015 Miquel Massot Campos
/// Systems, Robotics and Vision
/// University of the Balearic Islands
/// All rights reserved.

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/image_encodings.h>
#include <cv_bridge/cv_bridge.h>

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


class VignettingRemover {
 public:
  VignettingRemover(ros::NodeHandle nh, ros::NodeHandle nhp): nh_(nh), nhp_(nhp), it_(nh) {
    string image_topic = ros::names::remap("image");
    ROS_INFO_STREAM("Subscribing to " << image_topic);

    string output_topic("image_vig");
    nhp.param("output_topic", output_topic, string("image_vig"));
    img_pub_ = it_.advertise(output_topic, 1);
    // Read the images
    string mean_filename, std_filename;
    nhp.param("mean_filename", mean_filename, string(""));
    nhp.param("std_filename",  std_filename,  string(""));
    ROS_INFO_STREAM("Reading mean image: " << mean_filename.c_str() <<
                    " and std image: " << std_filename.c_str());
    Mat mean_uchar = imread(mean_filename, CV_LOAD_IMAGE_GRAYSCALE);
    Mat std_uchar  = imread(std_filename,  CV_LOAD_IMAGE_GRAYSCALE);

    // Convert them to double
    Mat mean_d(mean_uchar.size(), CV_64FC1);
    Mat std_d(std_uchar.size(), CV_64FC1);
    mean_uchar.convertTo(mean_d, CV_64F);
    std_uchar.convertTo(std_d, CV_64F);
    avg_ = mean_d;

    // Compute gain and offset for posterior correction
    Mat desired_avg = 0.5*Mat::ones(mean_d.size(), CV_64FC1);
    double desired_sigma = 1.0/9;
    divide(Mat::ones(std_d.size(), CV_64FC1), std_d, gain_, desired_sigma);
    offset_ = desired_avg - mean_d;
    ROS_INFO_STREAM("Gain and offset ready!");


    img_sub_ = it_.subscribe(image_topic,
                             1, // queue size
                             &VignettingRemover::imageCallback,
                             this); // transport
  }

 private:
  Mat gain_;
  Mat offset_;
  Mat avg_;
  ros::NodeHandle nh_;
  ros::NodeHandle nhp_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber img_sub_;
  image_transport::Publisher img_pub_;

  void imageCallback(
    const sensor_msgs::ImageConstPtr &image_msg) {
    cv_bridge::CvImagePtr cv_image_ptr;

    ROS_INFO("CALLBACK ENTERED");

    try {
      cv_image_ptr = cv_bridge::toCvCopy(image_msg,
                                         sensor_msgs::image_encodings::MONO8);
    } catch (cv_bridge::Exception& e) {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // Convert image to double
    Mat img = cv_image_ptr->image;
    Mat img_d(img.size(), CV_64FC1);
    img.convertTo(img_d, CV_64F);

    // Correct
    Mat corrected;
    multiply(img_d-avg_, gain_, corrected);
    corrected = corrected + avg_ + offset_;

    sensor_msgs::ImagePtr out_msg = cv_bridge::CvImage(image_msg->header,
                                                       "mono8",
                                                       corrected).toImageMsg();
    img_pub_.publish(out_msg);
  }
};


int main(int argc, char **argv)
{
  ros::init(argc, argv, "vignetting_remover");
  ros::NodeHandle nh;
  ros::NodeHandle nhp("~");
  VignettingRemover(nh, nhp);
  ros::spin();
}