#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <random>
#include <iostream>
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include <angles/angles.h>
#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/path.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

class fissionFusion : public rclcpp::Node
{
public:
    fissionFusion() : Node("fissionFusion")
    {
        // subscription
        pose_subscription_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "pose", 10, std::bind(&fissionFusion::pose_callback, this, std::placeholders::_1));
        target_pose_subscription_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/target_pose", 10, std::bind(&fissionFusion::target_pose_callback, this, std::placeholders::_1));
        cmd_vel_subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "cmd_vel", 10, std::bind(&fissionFusion::cmd_vel_callback, this, std::placeholders::_1));
        rad_sensor_subscription_ = this->create_subscription<std_msgs::msg::Float64MultiArray>(
            "rab_sensor", 10, std::bind(&fissionFusion::rab_sensor_callback, this, std::placeholders::_1));

        // publisher
        path_publisher_ = this->create_publisher<nav_msgs::msg::Path>("path_history", 10);
        predict_path_publisher_ = this->create_publisher<nav_msgs::msg::Path>("path_predict", 10);
        cmd_vel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
        odom_publisher_ = this->create_publisher<nav_msgs::msg::Odometry>("odom", 10);
        rab_actuator_publisher_ = this->create_publisher<std_msgs::msg::Float64MultiArray>("rab_actuator", 10);
        target_pose_publisher_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("target_pose", 10);

        this->declare_parameter<std::string>("controller_type", "SDRM");

        std::string controller_type = this->get_parameter("controller_type").as_string();

        if (controller_type == "SDRM")
        {
            current_controller_ = std::bind(&fissionFusion::SDRM_controller_step, this);
        }
        else if (controller_type == "P")
        {
            current_controller_ = std::bind(&fissionFusion::P_control_step, this);
        }
        else if (controller_type == "skybat")
        {
            current_controller_ = std::bind(&fissionFusion::skybat_controller_step, this);
        }
        else
        {
            RCLCPP_WARN(this->get_logger(), "Unknown controller type. Defaulting to SDRM.");
            current_controller_ = std::bind(&fissionFusion::SDRM_controller_step, this);
        }

        std::random_device rd;
        std::default_random_engine rng(rd());

        timerA_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&fissionFusion::visualization, this));

        timerB_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            current_controller_);

    }

private:
    geometry_msgs::msg::PoseStamped current_pose;
    geometry_msgs::msg::PoseStamped target_pose;
    nav_msgs::msg::Odometry odom;
    geometry_msgs::msg::Twist current_vel;
    std_msgs::msg::Float64MultiArray rab_data;
    nav_msgs::msg::Path path_msg;
    nav_msgs::msg::Path path_predict;

    void pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
    {
        current_pose = *msg;
        current_pose.header.frame_id = "map";
    }
    void target_pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
    {
        target_pose = *msg;
        target_pose.header.frame_id = "map";
    }

    void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
    {
        current_vel = *msg;
    }
    void rab_sensor_callback(const std_msgs::msg::Float64MultiArray::SharedPtr msg)
    {
        rab_data = *msg;
    }

    /*************************************************************************
     * Visualization
     **************************************************************************/
    void visualization();
    void publish_path();
    void publish_predict_path();
    void publish_odometry();

    /*************************************************************************
     * P controller
     **************************************************************************/
    double multi_hop_internet;
    bool isRabActuatorActive;
    bool isPerceiveActive;
    bool isEstimateActive;

    void P_control_step();
    void estimate_target_pose();
    void Perceive_target_pose();
    void publish_rab_actuator();
    void velocityGenerator();

    /*************************************************************************
     * SDRM Controller---Social Roosting Decision Model (SRDM)
     **************************************************************************/
    double SDRM_linear_velocity;
    double SDRM_angular_velocity;
    double lambda_random_ = 0.95;
    double lambda_social_ = 22;
    
    rclcpp::Time next_trigger_time_random_ = this->get_clock()->now();
    rclcpp::Time next_trigger_time_social_ = this->get_clock()->now();
    std::string current_decision_;



    void SDRM_controller_step();
    double generate_exponential(double lambda);
    void SDRM_random_walk();
    void SDRM_social_influence();
    void SDRM_poisson_process();
    void SDRM_publish_velocity();

    /*************************************************************************
     * skybat controller
     **************************************************************************/
    void skybat_controller_step();

    // subscriper
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr target_pose_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_subscription_;
    rclcpp::Subscription<std_msgs::msg::Float64MultiArray>::SharedPtr rad_sensor_subscription_;
    // publisher
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_publisher_;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr predict_path_publisher_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_publisher_;
    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr rab_actuator_publisher_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr target_pose_publisher_;

    rclcpp::TimerBase::SharedPtr timerA_;
    rclcpp::TimerBase::SharedPtr timerB_;
    std::function<void()> current_controller_;
};
