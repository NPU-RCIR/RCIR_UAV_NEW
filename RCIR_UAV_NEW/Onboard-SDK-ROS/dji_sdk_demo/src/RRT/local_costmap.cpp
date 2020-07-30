#include <RRT/local_costmap.h>

namespace RRT {

LocalCostmap::LocalCostmap(ros::NodeHandle &nh, ros::NodeHandle pnh)
{  
    occ_grid_pub = nh.advertise<nav_msgs::OccupancyGrid>("/local_planner/local_costmap", 100);
    obstacle_sub = nh.subscribe<cam::mono_radar_fusion>("/mono_radar/obstacle/msg", 10, &LocalCostmap::ObstaclesCallback, this);
    double costmap_height_meters;
    double costmap_width_meters;
    pnh.getParam("local_costmap_res", m_local_costmap_res);
    pnh.getParam("local_costmap_height", costmap_height_meters);
    pnh.getParam("local_costmap_width", costmap_width_meters);
    m_local_costmap_height = costmap_height_meters / m_local_costmap_res;
    m_local_costmap_width = costmap_width_meters / m_local_costmap_res;
    setupCostmap();
}

LocalCostmap::~LocalCostmap()
{
    ROS_INFO_STREAM("Local Costmap Destructor Called");
}

void LocalCostmap::setupCostmap()
{    
    occ_grid.header.frame_id = "map";
    occ_grid.info.resolution = m_local_costmap_res;
    occ_grid.info.origin.position.x = -m_local_costmap_height / 10;
    occ_grid.info.origin.position.y = -m_local_costmap_width / 10;
    occ_grid.info.origin.position.z = 0;
    tf::Quaternion q = tf::createQuaternionFromRPY(0, M_PI, -M_PI / 2);
    occ_grid.info.origin.orientation.w = q.getW();
    occ_grid.info.origin.orientation.x = q.getX();
    occ_grid.info.origin.orientation.y = q.getY();
    occ_grid.info.origin.orientation.z = q.getZ();
    occ_grid.info.height = m_local_costmap_height;
    occ_grid.info.width = m_local_costmap_width;
    m_grid_array_length = int(m_local_costmap_height * m_local_costmap_width);
    clearMap();
}

void LocalCostmap::createObstacles()
{
    std::vector<std::vector<points>> obstacle_points;
//    obstacle_points.push_back(createRectangle(20, 30, point(65, 30)));
//    obstacle_points.push_back(createRectangle(10, 35, point(65, 40)));
//    //obstacle_points.push_back(createRectangle(25, 25, point(-20, -50)));
//    obstacle_points.push_back(createRectangle(20, 30, point(-10, 43)));
//    obstacle_points.push_back(createRectangle(10, 35, point(60, 30)));
//    obstacle_points.push_back(createRectangle(25, 25, point(-20, 10)));
    //std::cout<<obsta_info.rects.size()<<std::endl;
    for(int i=0; i<obsta_info.rects.size(); i++)
    {
      double height = obsta_info.rects[i].ymax - obsta_info.rects[i].ymin + 1.2;
      double width  = obsta_info.rects[i].xmax - obsta_info.rects[i].xmin + 1.2;
      double x_i    = (obsta_info.rects[i].xmax + obsta_info.rects[i].xmin)/2;
      double y_i    = (obsta_info.rects[i].ymax + obsta_info.rects[i].ymax)/2;
      obstacle_points.push_back(createRectangle(height, width, points(x_i, y_i)));
      //std::cout<<"height: "<<height<<"---width: "<<width<<std::endl;
      //std::cout<<"x_i: "<<x_i<<"---y_i: "<<y_i<<std::endl;
    }
    clearMap();
    for(auto obstacle : obstacle_points)
    {
        for(auto pt : obstacle)
        {
            int location = calcGridLocation(pt.first, pt.second);
            occ_grid.data[location] = 100;
        }
    }
}

std::vector<points> LocalCostmap::createRectangle(const double &height, const double &width, const points &center)
{
    std::vector<points> rectangle_points;
    int num_pts_x = width / m_local_costmap_res * 2;
    int num_pts_y = height / m_local_costmap_res * 2;
    double start_x = center.first - width / 2;
    double start_y = center.second - height / 2;
    for(int i = 0; i < num_pts_x; i++)
    {
        for(int j = 0; j < num_pts_y; j++)
        {
            double x = start_x + i / double(num_pts_x) * width;
            double y = start_y + j / double(num_pts_y) * height;
            rectangle_points.push_back(points(y, x));
        }
    }
    return rectangle_points;
}

std::vector<points> LocalCostmap::calcPointsBetween(const points &pt1, const points &pt2)
{
    std::vector<points> points_between;
    double d_x = pt2.first - pt1.first;
    double d_y = pt2.second - pt1.second;
    double dist_between = sqrt(pow(d_x, 2) + pow(d_y, 2));
    int num_pts_between = dist_between / m_local_costmap_res * 2;
    double angle_between = atan2(d_y, d_x);
    for(int i = 0; i < num_pts_between; i++)
    {
        int x = pt1.first + i / double(num_pts_between) * dist_between * cos(angle_between);
        int y = pt1.second + i / double(num_pts_between) * dist_between * sin(angle_between);
        points pt = std::make_pair(x, y);
        points_between.push_back(pt);
    }
    return points_between;
}

//void LocalCostmap::calcOccGrid()
//{
//    occ_grid.header.stamp = ros::Time::now();
//    clearMap();
//    createObstacles();
//}


int LocalCostmap::calcGridLocation(const double &x, const double &y)
{
    int width_pos = int(x / m_local_costmap_res + m_local_costmap_width / 2);
    int height_pos = int(y / m_local_costmap_res + m_local_costmap_height / 2);
    int location = width_pos + height_pos * m_local_costmap_width - 1;
    return location;
}

points LocalCostmap::calcCartesianCoords(const int &location)
{
    std::div_t result = std::div(location, m_local_costmap_height);
    int height_pos = result.quot;
    int width_pos = result.rem;
    double x = (width_pos - m_local_costmap_width / 2) * m_local_costmap_res;
    double y = (height_pos - m_local_costmap_height / 2) * m_local_costmap_res;
    points coords = std::make_pair(x, y);
    return coords;
}

void LocalCostmap::clearMap()
{
    occ_grid.data.clear();
    for(int i = 0; i < m_grid_array_length; i++)
    {
        occ_grid.data.push_back(-1);
    }
}

void LocalCostmap::pubOccGrid()
{
    createObstacles();
    occ_grid_pub.publish(occ_grid);
}

void LocalCostmap::ObstaclesCallback(const cam::mono_radar_fusionConstPtr &obsta)
{
  obsta_info.rects = obsta->rects;
}

}
