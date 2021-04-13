#pragma once
#define RRT_HDR
#include <stdlib.h>
#include <vector>
#include <list>
#include <memory>
#include <math.h>
#include <Eigen/Dense>
#include <random>
#ifndef OBSTACLES
#include "obstacles.hpp"
#endif
#ifndef GRAPH_HDR
#include "graph.hpp"
#endif
#include <assert.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#ifndef CONFIG_HDR
#include "../include/configfile.hpp"
#endif

using namespace std;

class RRT : public Graph {
public:
    Obstacles obstacles;
    const double MAP_WIDTH;
    const double MAP_HEIGHT;
    const double ROBOT_RADIUS;
    const double HEIGHT_RATIO;
    const double WIDTH_RATIO;
    //Obstacles obstacle;
    RRT(shared_ptr<Node> begining, shared_ptr<Node> target,  Obstacles obs, double map_width, double map_height, double step_size=0.07, int max_iter=2000, double epsilon=0.001, double robot_radius=0.009, double width_ratio=1, double height_ratio=1):
        Graph(begining, target),
        MAP_WIDTH(map_width),
        MAP_HEIGHT(map_height),
        STEP_SIZE(step_size),
        MAX_ITER(max_iter),
        EPSILON(epsilon),
        ROBOT_RADIUS(robot_radius),
        HEIGHT_RATIO(height_ratio),
        WIDTH_RATIO(width_ratio) {
        begining->set_id(1);
        std::cout << "RRT instantiated, map_width: " << map_width << ", map_height: " << map_height << ", step_size: " << step_size << ", max_iter: " << max_iter << ", epsilon: " << epsilon << ", robot_radius: " << robot_radius << std::endl;
        add_node(begining);
        std::cout << "begining added!" << std::endl;
        for (int i =0; i < obs.size(); i++) {
            obstacles.push_back(obs[i]);
        }
        std::cout << "RRT inistantiated!" << std::endl;
    }

    void search() {
        int c=0;
        while (!reached() && c++<MAX_ITER) {
            shared_ptr<Node> rand = getWindowedRandomNode();
            shared_ptr<Node> near=nearest(rand);
            shared_ptr<Node> sample=newConfig(rand, near);
            if (!intersect(near, sample)) {
                add(near, sample);
            }
        }
        auto target = get_end();
        auto near_to_target=nearest(target);
        add(near_to_target, target); //add target to the nodes, and complete the graph
    }

protected:
    /** 2D sampling
     *
    */
    shared_ptr<Node> getRandomNode() {
        double x, y;
        Eigen::VectorXd  p_vec(2);
        std::random_device rd;
        x=rd();
        y=rd();
        //demean(shift) the point to the center of the map at (0,0)
        x -= (rd.max() - rd.min())/2;
        y -= (rd.max() - rd.min())/2;
        //scale point to map boundaries
        x = x/rd.max() * MAP_WIDTH;
        y = y/rd.max() * MAP_HEIGHT;
        //
        p_vec << x, y;
        return make_shared<Node>(p_vec);
    }
    shared_ptr<Node> getWindowedRandomNode() {
        auto target = get_end();
        auto last = nearest(target);
        // you need the nearest to the target
        auto last_pt = last->get_point().vector();
        double X = last_pt(0);
        double Y = last_pt(1);
        //
        double x, y;
        Eigen::VectorXd  p_vec(2);
        std::random_device rd;
        do {
            x=rd();
            y=rd();
            //demean(shift) the point to the center of the map at (0,0)
            x -= (rd.max() - rd.min())/2;
            y -= (rd.max() - rd.min())/2;
            //scale point to map boundaries
            //TODO (impl) if the target on the right add, otherwise subtract from (X,Y)
            x = x/rd.max() * MAP_WIDTH/4 + X;
            y = y/rd.max() * MAP_HEIGHT/4 + Y;
        } while (!(x<MAP_WIDTH/(2*WIDTH_RATIO) || x>-1*MAP_WIDTH/(2*HEIGHT_RATIO)) ||
                 !(y<MAP_HEIGHT/2 || y>-1*MAP_HEIGHT/2));
        p_vec << x, y;
        return make_shared<Node>(p_vec);
    }
    //
    shared_ptr<Node> nearest(shared_ptr<Node> target) {
        double min=1e9; //TODO (enh) add this to configuration
        shared_ptr<Node> closest(nullptr);
        shared_ptr<Node> cur(nullptr);
        double dist;
        auto G = get_nodes();
        for (auto &&node : G) {
            cur=node;
            dist = distance(target, cur);
            if (dist < min) {
                min=dist;
                closest=cur;
            }
        }
        return closest;
    }
    //such that newconfig is at x% of the distance
    //so you can calculate it.
    shared_ptr<Node> newConfig(shared_ptr<Node> q, shared_ptr<Node> qNearest) {
        double x,y;
        x=INFINITY;
        y=INFINITY;
        Eigen::VectorXd to = q->get_point().vector();
        Eigen::VectorXd from = qNearest->get_point().vector();
        Eigen::VectorXd intermediate = to - from;
        intermediate = intermediate / intermediate.norm();
        Eigen::VectorXd ret;
        double tmp_step=STEP_SIZE;
        do {
            ret = from + tmp_step * intermediate;
            x = ret(0);
            y = ret(1);
            tmp_step/=2;
        } while ((x > MAP_WIDTH/2 || x < -1*MAP_WIDTH/2) ||
                 (y > MAP_HEIGHT/2 || y < -1*MAP_HEIGHT/2));
        return make_shared<Node>(ret);
    }
    bool intersect(shared_ptr<Node> near, shared_ptr<Node> sample) {
        bool intersect=false;
        for (int i = 0; i < obstacles.size() && !intersect; i++) {
            intersect=obstacles[i]->intersect(near->get_point(),
                                              sample->get_point(),
                                              ROBOT_RADIUS);
        }
        return intersect;
    }

    void add(shared_ptr<Node> parent, shared_ptr<Node> sampled) {
        int new_index = get_last()->get_id()+1;
        sampled->set_id(new_index);
        assert(!sampled->has_parent());
        sampled->set_parent(parent);
        add_node(sampled);
        parent->add_edge(sampled);
        std::cout << "sampled: " << *sampled << std::endl;
    }
    /** wither it reached the target node
     *
     * verify the distance between the target and current node = 0
     * @return true if reached, false otherwise.
     */
    bool reached() {
        shared_ptr<Node> target = get_end();
        shared_ptr<Node> pos = get_last();
        auto dist = distance(target, pos);
        return (dist<=EPSILON)?true:false;
    }

    const double STEP_SIZE;
    const int MAX_ITER;
    const double EPSILON;
};

class ConRRT : public ConGraph {
    std::mutex mu;
    std::condition_variable cond;
    std::vector<shared_ptr<Node>> frontline;
public:
    Obstacles obstacles;
    const double MAP_WIDTH;
    const double MAP_HEIGHT;
    const double ROBOT_RADIUS;
    const double HEIGHT_RATIO;
    const double WIDTH_RATIO;
    //Obstacles obstacle;
    ConRRT(vector<shared_ptr<Node>> begining, shared_ptr<Node> target, Obstacles obs, double map_width, double map_height, double step_size=0.07, int max_iter=3000, int epsilon=0.01, double robot_radius=0.009, double width_ratio=1, double height_ratio=1) :
        ConGraph(begining, target),
        MAP_WIDTH(map_width),
        MAP_HEIGHT(map_height),
        STEP_SIZE(step_size),
        MAX_ITER(max_iter),
        EPSILON(epsilon),
        ROBOT_RADIUS(robot_radius),
        WIDTH_RATIO(width_ratio),
        HEIGHT_RATIO(height_ratio) {
        for (auto &&node : begining) {
            add_node(node);
            frontline.push_back(node);
        }
        for (int i =0; i < obs.size(); i++) {
            obstacles.push_back(obs[i]);
        }
        std::cout << "RRT inistantiated!" << std::endl;
    }

    /* search
     *
     * search path from node to the target
     * @param st : starting node
     */
    void search(shared_ptr<Node> pos) {
        int c=0;
        //while (!reached(pos) && c++<MAX_ITER) {
        while (!reached(pos)) {
            auto rand = getWindowedRandomNode();
            auto near=nearest(rand);
            auto sample=newConfig(rand, near);
            bool intersect=false;
            for (int i = 0; i < obstacles.size() && !intersect; i++) {
                intersect=obstacles[i]->intersect(near->get_point(),
                                                  sample->get_point(),
                                                  ROBOT_RADIUS);
            }
            if(!intersect) {
                // if the new configuration sample doens't intersect
                // with the line near-sample, then add it to the graph.
                add(near, sample);
                pos=sample;
            }
        }
    }
    void run() {
        int c=0;
        //TODO add threads with promises of position in list, then assign value later
        std::vector<std::thread> pool;
        for (auto &&pos : frontline) {
            std::thread th(&ConRRT::search, this,  pos);
            pool.push_back(std::move(th));
        }
        for (int i=0; i < pool.size(); i++) {
            if (pool[i].joinable())
                pool[i].join();
        }
        auto target = get_end();
        auto near_to_target=nearest(target);
        add(near_to_target, target);
        std::cout << "finished!" << std::endl;
    }

protected:
  /** 2D sampling
   *
   */
  //TODO (impl) 3d sampling
  shared_ptr<Node> getRandomNode() {
    //std::cout << "getrandomnode" << std::endl;
    /*
      Node* ret;
      Vector2f point(drand48() * WORLD_WIDTH, drand48() * WORLD_HEIGHT);
      if (point.x() >= 0 && point.x() <= WORLD_WIDTH && point.y() >= 0 && point.y() <= WORLD_HEIGHT) {
      ret = new Node;
      ret->position = point;
      return ret;
      }
      return NULL;
    */
    double x, y;
    std::random_device rd;
    x=rd();
    y=rd();
    //std::cout << "getrandomNodes: " << "randomly sampled (" << x << "," << y << ")" << std::endl;
    //demean(shift) the point to the center of the map at (0,0)
    x -= (rd.max() - rd.min())/2;
    y -= (rd.max() - rd.min())/2;
    //scale point to map boundaries
    x = x/rd.max() * MAP_WIDTH;
    y = y/rd.max() * MAP_HEIGHT;

    //assert(x <= MAP_WIDTH/2 && x >= -1*MAP_WIDTH/2);
    //assert(y <= MAP_HEIGHT/2 && y >= -1*MAP_HEIGHT/2);

    Eigen::VectorXd  p_vec(2);
    p_vec << x, y;
    //std::cout << "scaled (" << x << "," << y << ")" << std::endl;
    return make_shared<Node>(p_vec);
  }

shared_ptr<Node> getWindowedRandomNode() {
        auto target = get_end();
        auto last = nearest(target);
        // you need the nearest to the target
        auto last_pt = last->get_point().vector();
        double X = last_pt(0);
        double Y = last_pt(1);
        //
        double x, y;
        Eigen::VectorXd  p_vec(2);
        std::random_device rd;
        do {
            x=rd();
            y=rd();
            //demean(shift) the point to the center of the map at (0,0)
            x -= (rd.max() - rd.min())/2;
            y -= (rd.max() - rd.min())/2;
            //scale point to map boundaries
            //TODO (impl) if the target on the right add, otherwise subtract from (X,Y)
            x = x/rd.max() * MAP_WIDTH/4 + X;
            y = y/rd.max() * MAP_HEIGHT/4 + Y;
        } while (!(x<MAP_WIDTH/(2*WIDTH_RATIO) || x>-1*MAP_WIDTH/(2*HEIGHT_RATIO)) ||
                 !(y<MAP_HEIGHT/2 || y>-1*MAP_HEIGHT/2));
        p_vec << x, y;
        return make_shared<Node>(p_vec);
    }
    //TODO (fix) adjust steps_size
    //such that newconfig is at x% of the distance
    //so you can calculate it.
    shared_ptr<Node> newConfig(shared_ptr<Node> q, shared_ptr<Node> qNearest) {
        double x,y;
        x=INFINITY;
        y=INFINITY;
        Eigen::VectorXd to = q->get_point().vector();
        Eigen::VectorXd from = qNearest->get_point().vector();
        Eigen::VectorXd intermediate = to - from;
        intermediate = intermediate / intermediate.norm();
        Eigen::VectorXd ret;
        /////////////////
        // TODO learn the highest step_size possible
        /////////////////
        double tmp_step=STEP_SIZE;
        do {
            ret = from + tmp_step * intermediate;
            x = ret(0);
            y = ret(1);
            tmp_step/=2;
        } while ((x > MAP_WIDTH/2 || x < -1*MAP_WIDTH/2) ||
                 (y > MAP_HEIGHT/2 || y < -1*MAP_HEIGHT/2));

        assert(x <= MAP_WIDTH/2 && x >= -1*MAP_WIDTH/2);
        assert(y <= MAP_HEIGHT/2 && y >= -1*MAP_HEIGHT/2);

        return make_shared<Node>(ret);
    }

    void add(shared_ptr<Node> parent, shared_ptr<Node> sampled) {
        int new_index = get_last()->get_id()+1;
        sampled->set_id(new_index);
        assert(!sampled->has_parent());
        sampled->set_parent(parent);
        add_node(sampled);
        parent->add_edge(sampled);
        std::cout << "sampled: " << *sampled << std::endl;
    }
    /** wither it reached the target node
     *
     * verify the distance between the target and current node = 0
     * @return true if reached, false otherwise.
     */
    bool reached(shared_ptr<Node> pos) {
        shared_ptr<Node> target = get_end();
        auto dist = distance(target, pos);
        return (dist <= EPSILON) ? true : false;
    }
    const double STEP_SIZE;
    const int MAX_ITER;
    const double EPSILON;
};
