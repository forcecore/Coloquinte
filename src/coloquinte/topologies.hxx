
#include "common.hxx"

#ifndef COLOQUINTE_TOPOLOGIES
#define COLOQUINTE_TOPOLOGIES

namespace coloquinte{

template<int pin_cnt>
struct Hconnectivity{
    // The edges and the couple of pins connected to the extreme ones are represented by one char each
    // The first 4 bits represent the first pin minus one, the next 4 bits the second pin minus one
    std::uint8_t connexions[pin_cnt-3];
    std::uint8_t extremes;

    typedef std::pair<index_t, index_t> edge_t;

    struct minmax_t{
        int_t min, max;

        minmax_t(int_t mn, int_t mx) : min(mn), max(mx) {}
        minmax_t() {}
        void merge(minmax_t const o){
            min = std::min(o.max, min);
            max = std::max(o.min, max);
        }
        void merge(int_t const p){
            min = std::min(p, min);
            max = std::max(p, max);
        }
    };

    int_t get_wirelength(std::array<point<int_t>, pin_cnt> const sorted_points) const{
        std::array<minmax_t, pin_cnt-2> minmaxs;
        for(index_t i=0; i<pin_cnt-2; ++i){
            minmaxs[i] = minmax_t(sorted_points[i+1].y_, sorted_points[i+1].y_);
        }
        std::uint8_t b_con = extremes & 15u, e_con = extremes >> 4;
        minmaxs[b_con].merge(sorted_points.front() .y_);
        minmaxs[e_con].merge(sorted_points.back()  .y_);
        for(std::uint8_t const E : connexions){
            minmaxs[(E >> 4)].merge(minmaxs[(E & 15u)]);
        }
        int_t cost = sorted_points.back().x_ - sorted_points.front().x_ + sorted_points[b_con+1].x_ - sorted_points[e_con+1].x_;
        for(std::uint8_t const E : connexions){
            cost += std::abs(sorted_points[(E >> 4) +1].x_ - sorted_points[(E & 15u) +1].x_);
        }
        for(index_t i=0; i<pin_cnt-2; ++i){
            cost += (minmaxs[i].max - minmaxs[i].min);
        }
        return cost;
    }

    std::array<edge_t, pin_cnt-1> get_x_topology(std::array<point<int_t>, pin_cnt> const sorted_points) const{
        std::array<edge_t, pin_cnt-1> ret;
        std::uint8_t b_con = extremes & 15u, e_con = extremes >> 4;
        ret[0] = edge_t(0, b_con+1);
        ret[1] = edge_t(pin_cnt-1, e_con+1);
        for(index_t i=0; i<pin_cnt-3; ++i){
            std::uint8_t E = connexions[i];
            ret[i+2] = edge_t((E & 15u) +1, (E >> 4) +1);
        }

        return ret;
    }
};

namespace steiner_lookup{
    extern std::array<Hconnectivity<4>, 2> const topologies_4;
    extern std::array<Hconnectivity<5>, 6> const topologies_5;
    extern std::array<Hconnectivity<6>, 23> const topologies_6;
    extern std::array<Hconnectivity<7>, 111> const topologies_7;
    extern std::array<Hconnectivity<8>, 642> const topologies_8;
    extern std::array<Hconnectivity<9>, 4334> const topologies_9;
    extern std::array<Hconnectivity<10>, 33510> const topologies_10;
}

}

#endif

