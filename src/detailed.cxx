
#include "coloquinte/detailed.hxx"
#include "coloquinte/circuit_helper.hxx"
#include "coloquinte/optimization_subproblems.hxx"

#include <lemon/smart_graph.h>
#include <lemon/network_simplex.h>

#include <map>
#include <cassert>

namespace coloquinte{
namespace dp{


detailed_placement::detailed_placement(
        placement_t pl,
        std::vector<index_t> placement_rows,
        std::vector<index_t> cell_heights,
        std::vector<std::vector<index_t> > rows,
        int_t min_x, int_t max_x,
        int_t y_origin,
        index_t nbr_rows, int_t row_height
    )
    :
        plt_(pl),
        cell_rows_(placement_rows),
        min_x_(min_x), max_x_(max_x),
        y_origin_(y_origin)
    {

    assert(row_height > 0);
    assert(min_x < max_x);
    assert(rows.size() == nbr_rows);

    neighbours_limits_.push_back(0); 
    for(index_t h : cell_heights){
        neighbours_limits_.push_back(neighbours_limits_.back() + h);
    }

    neighbours_ .resize(neighbours_limits_.back(), std::pair<index_t, index_t>(null_ind, null_ind) );

    row_first_cells_ .resize(nbr_rows, null_ind);
    row_last_cells_  .resize(nbr_rows, null_ind);

    std::vector<bool> explored(neighbours_limits_.back(), false);
    // Now we extract the dependencies
    for(index_t r=0; r<rows.size(); ++r){

        if(not rows[r].empty()){
            row_first_cells_[r] = rows[r].front();
            row_last_cells_[r]  = rows[r].back();
        }

        for(index_t c : rows[r]){
            // Has this row of the cell already been visited?
            assert(not explored[neighbour_index(c, r)]);
            explored[neighbour_index(c, r)] = true;
        }

        for(index_t i=0; i+1<rows[r].size(); ++i){
            index_t c1 = rows[r][i], c2 = rows[r][i+1];

            // Save in the internal format
            neighbours_[neighbour_index(c1, r)].second = c2;
            neighbours_[neighbour_index(c2, r)].first  = c1;

            // The positions are correct
        }
    }

    // Every level of every cell must have been visited
    for(bool o : explored)
        assert(o);

    // Verify that we haven't made any obvious mistake
    selfcheck();
}

void detailed_placement::selfcheck() const{
    assert(row_first_cells_.size() == row_last_cells_.size());

    for(index_t i=0; i<cell_cnt(); ++i){
        for(index_t l=0; l<cell_height(i); ++l){
            // not verified now since we don't modify the position for the obstacles
            // : assert(c.position.x_ >= min_x_ and c.position.x_ + c.width <= max_x_);

            index_t n_ind = l + neighbours_limits_[i];
            assert(cell_rows_[i] + cell_height(i) <= row_cnt());

            if(neighbours_[n_ind].first != null_ind){
                index_t oi = neighbours_[n_ind].first;
                // Correct neighbour position
                assert(neighbours_[neighbour_index(oi, cell_rows_[i]+l)].second == i);
            }
            else{
                // Beginning of a row
                assert(row_first_cells_[cell_rows_[i] + l] == i);
            }
            if(neighbours_[n_ind].second != null_ind){
                index_t oi = neighbours_[n_ind].second;
                // Correct neighbour position
                assert(neighbours_[neighbour_index(oi, cell_rows_[i]+l)].first == i);
            }
            else{
                // End of a row
                assert(row_last_cells_[cell_rows_[i] + l] == i);
            }
        }
    }
}

void detailed_placement::swap_topologies(index_t c1, index_t c2){
    assert(cell_height(c1) == cell_height(c2));
    assert(cell_height(c1) == 1 and cell_height(c2) == 1);

    index_t row_c1 = cell_rows_[c1],
            row_c2 = cell_rows_[c2];

    index_t b_c1 = neighbours_[neighbours_limits_[c1]].first;
    index_t b_c2 = neighbours_[neighbours_limits_[c2]].first;
    index_t a_c1 = neighbours_[neighbours_limits_[c1]].second;
    index_t a_c2 = neighbours_[neighbours_limits_[c2]].second;

    // Two cases: they were adjacent or they were not
    // Luckily updating in the neighbours first then swapping the recorded neighbours works in both cases for standard cells

    // Update the pointers in the cells' neighbours
    if(b_c1 != null_ind) neighbours_[neighbour_index(b_c1, row_c1)].second = c2;
    else row_first_cells_[row_c1] = c2;
    if(b_c2 != null_ind) neighbours_[neighbour_index(b_c2, row_c2)].second = c1;
    else row_first_cells_[row_c2] = c1;

    if(a_c1 != null_ind) neighbours_[neighbour_index(a_c1, row_c1)].first  = c2;
    else row_last_cells_[row_c1] = c2;
    if(a_c2 != null_ind) neighbours_[neighbour_index(a_c2, row_c2)].first  = c1;
    else row_last_cells_[row_c2] = c1;

    // Swap the properties in both cells
    std::swap(neighbours_[neighbours_limits_[c1]], neighbours_[neighbours_limits_[c2]]);
    std::swap(cell_rows_[c1], cell_rows_[c2]);
}

std::pair<int_t, int_t> detailed_placement::get_limit_positions(netlist const & circuit, index_t c){
    auto ret = std::pair<int_t, int_t>(min_x_, max_x_);
    for(index_t l=neighbours_limits_[c]; l<neighbours_limits_[c+1]; ++l){
        index_t b_i = neighbours_[l].first,
                a_i = neighbours_[l].second;
        if(b_i != null_ind){
            ret.first  = std::max(ret.first,  plt_.positions_[b_i].x_ + circuit.get_cell(b_i).size.x_);
        }
        if(a_i != null_ind){
            ret.second = std::min(ret.second, plt_.positions_[a_i].x_);
        }
    }
    return ret;
}

index_t detailed_placement::get_first_cell_on_row(index_t r){
    index_t c = row_first_cells_[r];
    while(c != null_ind and cell_height(c) != 1){
        index_t next_c = neighbours_[neighbour_index(c, r)].second;
        assert(c != next_c);
        c = next_c;
    }
    assert(c == null_ind or cell_rows_[c] == r);
    return c;
}

index_t detailed_placement::get_next_cell_on_row(index_t c, index_t r){
    do{
        index_t next_c = neighbours_[neighbour_index(c, r)].second;
        assert(c != next_c);
        c = next_c;
    }while(c != null_ind and cell_height(c) != 1);
    assert(c == null_ind or cell_rows_[c] == r);
    return c;
}

void detailed_placement::reorder_standard_cells(std::vector<index_t> const old_order, std::vector<index_t> const new_order){
    assert(old_order.size() == new_order.size());
    assert(not old_order.empty());

    index_t before_row = neighbours_[neighbours_limits_[old_order.front()]].first;
    index_t after_row  = neighbours_[neighbours_limits_[old_order.back() ]].second;

    index_t r = cell_rows_[new_order.front()];

    for(index_t i=0; i<new_order.size(); ++i){
        assert(cell_height(new_order[i]) == 1);
        assert(cell_rows_[new_order[i]] == r);

        auto & nghs = neighbours_[neighbours_limits_[new_order[i]]];
        if(i > 0){
            nghs.first = new_order[i-1];
        }
        else{
            nghs.first = before_row;
        }
        if(i+1 < new_order.size()){
            nghs.second = new_order[i+1];
        }
        else{
            nghs.second = after_row;
        }
    }

    if(before_row != null_ind) neighbours_[neighbour_index(before_row, r)].second = new_order.front();
    else row_first_cells_[r] = new_order.front();
    if(after_row != null_ind) neighbours_[neighbour_index(after_row, r)].first = new_order.back();
    else row_last_cells_[r] = new_order.back(); 
}

void optimize_on_topology_HPWL(netlist const & circuit, detailed_placement & pl){
    // Solves a minimum cost flow problem to optimize the placement at fixed topology
    // Concretely, it means aligning the pins to minimize the wirelength
    // It uses the Lemon network simplex solver from the Coin-OR initiative, which should scale well up to hundred of thousands of cells

    using namespace lemon;
    DIGRAPH_TYPEDEFS(SmartDigraph);
    // Create a graph with the cells and bounds of the nets as node
    SmartDigraph g;

    std::vector<Node> cell_nodes(circuit.cell_cnt());
    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        if((circuit.get_cell(i).attributes & XMovable) != 0)
            cell_nodes[i] = g.addNode();
    }
    std::vector<Node> Lnet_nodes(circuit.net_cnt()), Unet_nodes(circuit.net_cnt());
    for(index_t i=0; i<circuit.net_cnt(); ++i){
        Lnet_nodes[i] = g.addNode();
        Unet_nodes[i] = g.addNode();
    }

    // Two nodes for position constraints
    Node fixed = g.addNode();

    typedef std::pair<SmartDigraph::Arc, int_t> arc_pair;
    typedef std::pair<SmartDigraph::Node, int_t> node_pair;
    // The arcs corresponding to constraints of the original problem
    std::vector<arc_pair> constraint_arcs;

    // Now we add every positional constraint, which becomes an arc in the min-cost flow problem
    for(index_t i=0; i<circuit.cell_cnt(); ++i){ // The cells
        for(index_t l = pl.neighbours_limits_[i]; l < pl.neighbours_limits_[i+1]; ++l){
            index_t oi = pl.neighbours_[l].second;
            if(oi == null_ind) continue;
            
            if((circuit.get_cell(i).attributes & XMovable) != 0 and (circuit.get_cell(oi).attributes & XMovable) != 0){
                // Two movable cells: OK
                auto A = g.addArc(cell_nodes[oi], cell_nodes[i]);
                constraint_arcs.push_back(arc_pair(A, -circuit.get_cell(i).size.x_));
            }
            else if((circuit.get_cell( i).attributes & XMovable) != 0){
                // The cell c is movable and constrained on the right
                auto A = g.addArc(fixed, cell_nodes[i]);
                constraint_arcs.push_back(arc_pair(A, pl.plt_.positions_[oi].x_ - circuit.get_cell(i).size.x_));
            }
            else if((circuit.get_cell(oi).attributes & XMovable) != 0){
                // The cell oc is movable and constrained on the left
                auto A = g.addArc(cell_nodes[oi], fixed);
                constraint_arcs.push_back(arc_pair(A, -pl.plt_.positions_[i].x_ - circuit.get_cell(i).size.x_));
            }
        }
    }

    
    for(index_t r=0; r<pl.row_cnt(); ++r){ // And the boundaries of each row
        index_t lc = pl.row_first_cells_[r];
        if(lc != null_ind and (circuit.get_cell(lc).attributes & XMovable) != 0){
            auto Al = g.addArc(cell_nodes[lc], fixed);
            constraint_arcs.push_back(arc_pair(Al, -pl.min_x_));
        }
    }
    for(index_t r=0; r<pl.row_cnt(); ++r){ // And the boundaries of each row
        index_t rc = pl.row_last_cells_[r];
        if(rc != null_ind and (circuit.get_cell(rc).attributes & XMovable) != 0){
            auto Ar = g.addArc(fixed, cell_nodes[rc]);
            constraint_arcs.push_back(arc_pair(Ar, pl.max_x_ - circuit.get_cell(rc).size.x_));
        }
    }
    

    // And every pin of every net: arcs too
    for(index_t n=0; n<circuit.net_cnt(); ++n){
        assert(circuit.get_net(n).pin_cnt > 0);
        for(auto p : circuit.get_net(n)){
            index_t c = p.cell_ind;
            int_t pin_offs = (pl.plt_.orientations_[c].x_ ? p.offset.x_ : circuit.get_cell(c).size.x_ - p.offset.x_); // Offset to the beginning of the cell
            if((circuit.get_cell(c).attributes & XMovable) != 0){
                Arc Al = g.addArc(cell_nodes[c], Lnet_nodes[n]);
                constraint_arcs.push_back(arc_pair(Al, pin_offs));
                Arc Ar = g.addArc(Unet_nodes[n], cell_nodes[c]);
                constraint_arcs.push_back(arc_pair(Ar, -pin_offs));
            }
            else{ // Fixed offset
                auto Al = g.addArc(fixed, Lnet_nodes[n]);
                constraint_arcs.push_back(arc_pair(Al, pl.plt_.positions_[c].x_ + pin_offs));
                auto Ar = g.addArc(Unet_nodes[n], fixed);
                constraint_arcs.push_back(arc_pair(Ar, - pl.plt_.positions_[c].x_ - pin_offs));
            }
        }
    }

    // Then the only capacitated arcs: the ones for the nets
    std::vector<node_pair> net_supplies;
    for(index_t n=0; n<circuit.net_cnt(); ++n){
        net_supplies.push_back(node_pair(Unet_nodes[n],  circuit.get_net(n).weight));
        net_supplies.push_back(node_pair(Lnet_nodes[n], -circuit.get_net(n).weight));
    }

    // Create the maps to have cost and capacity for the arcs
    IntArcMap cost(g, 0);
    IntArcMap capacity(g, circuit.net_cnt());
    IntNodeMap supply(g, 0);

    for(arc_pair A : constraint_arcs){
        cost[A.first] = A.second;
    }

    for(node_pair N : net_supplies){
        supply[N.first] = N.second;
    }

    // Then we (hope the solver can) solve it
    NetworkSimplex<SmartDigraph> ns(g);
    ns.supplyMap(supply).costMap(cost);
    auto res = ns.run();
    if(res != ns.OPTIMAL){
        abort();
    }
    
    // And we get the new positions as the dual values of the current solution (compared to the fixed pin) 
    for(index_t c=0; c<circuit.cell_cnt(); ++c){ // The cells
        if((circuit.get_cell(c).attributes & XMovable) != 0){
            pl.plt_.positions_[c].x_ = ns.potential(cell_nodes[c]) - ns.potential(fixed);
        }
    }
    pl.selfcheck();
}

namespace{

// Tries to swap two cells; 
inline bool try_swap(netlist const & circuit, detailed_placement & pl, index_t c1, index_t c2,
std::function<std::int64_t(netlist const &, detailed_placement const &, std::vector<index_t> const &)> get_nets_cost){
    assert(pl.cell_height(c1) == 1 and pl.cell_height(c2) == 1);
    assert(circuit.get_cell(c1).size.y_ == circuit.get_cell(c2).size.y_); // Same (standard cell) height
    assert( (circuit.get_cell(c1).attributes & XMovable) != 0 and (circuit.get_cell(c1).attributes & YMovable) != 0);
    assert( (circuit.get_cell(c2).attributes & XMovable) != 0 and (circuit.get_cell(c2).attributes & YMovable) != 0);

    auto c1_bnds = pl.get_limit_positions(circuit, c1),
         c2_bnds = pl.get_limit_positions(circuit, c2);

    // Get the possible positions for a swap
    int_t swp_min_c1 = c2_bnds.first,
          swp_min_c2 = c1_bnds.first,
          swp_max_c1 = c2_bnds.second - circuit.get_cell(c1).size.x_,
          swp_max_c2 = c1_bnds.second - circuit.get_cell(c2).size.x_;

    if(swp_max_c1 >= swp_min_c1 and swp_max_c2 >= swp_min_c2){
        // Check both orientations of the cell

        // Get all the nets involved and uniquify them (nets with more than one pin on the cells)
        std::vector<index_t> involved_nets;
        for(netlist::pin_t p : circuit.get_cell(c1)){
            involved_nets.push_back(p.net_ind);
        }
        for(netlist::pin_t p : circuit.get_cell(c2)){
            involved_nets.push_back(p.net_ind);
        }
        std::sort(involved_nets.begin(), involved_nets.end());
        involved_nets.resize(std::distance(involved_nets.begin(), std::unique(involved_nets.begin(), involved_nets.end())));

        // Test the cost for the old position and the cost swapping the cells
        std::int64_t old_cost = get_nets_cost(circuit, pl, involved_nets);

        // Save the old values
        point<int_t> p1 = pl.plt_.positions_[c1];
        point<int_t> p2 = pl.plt_.positions_[c2];

        // Warning: won't work if the two cells don't have the same height
        pl.plt_.positions_[c1].x_ = (swp_min_c1 + swp_max_c1) / 2;
        pl.plt_.positions_[c2].x_ = (swp_min_c2 + swp_max_c2) / 2;
        pl.plt_.positions_[c1].y_ = p2.y_;
        pl.plt_.positions_[c2].y_ = p1.y_;

        std::int64_t swp_cost = get_nets_cost(circuit, pl, involved_nets);

        if(swp_cost < old_cost){
            pl.swap_topologies(c1, c2);

            // We kept the swap
            return true;
        }
        else{
            // Reset the old values
            pl.plt_.positions_[c1] = p1;
            pl.plt_.positions_[c2] = p2;

            // We didn't swap
            return false;
        }

        // A better solution would be
        // Check the cost on y depending on the position (extremely simple: two positions for each cell)
        // Check the cost on x depending on the position: piecewise linear and relatively complex
        //      * Get all external pins
        //      * Get all nets involving only one of the cells: piecewise linear cost for each of them
        //      * For nets involving the two cells, we have an additional cost

    }
    else{ // We just cannot swap those two cells without pushing anything
        return false;
    }
}

struct net_properties{
    int_t ext_pos_min, ext_pos_max, weight;
    index_t cell_fst, cell_lst;
    std::pair<int_t, int_t> offs_fst, offs_lst;
    bool external_pins;

    net_properties(){
        weight = 1;
        ext_pos_min = std::numeric_limits<int_t>::max();
        ext_pos_max = -std::numeric_limits<int_t>::min();
        cell_fst = std::numeric_limits<index_t>::max();
        cell_lst = 0;
        auto empty_pins = std::pair<int_t, int_t>(ext_pos_min, ext_pos_max);
        offs_fst = empty_pins;
        offs_lst = empty_pins;
        external_pins = false;
    }
};

std::vector<net_properties> get_net_properties(netlist const & circuit, detailed_placement const & pl, std::vector<index_t> const & cells){
    struct order_gettr{
        index_t cell_ind, seq_order;
        bool operator<(order_gettr const o) const{ return cell_ind < o.cell_ind; }
        bool operator<(index_t const o) const{ return cell_ind < o; }
        order_gettr(index_t c, index_t i) : cell_ind(c), seq_order(i) {}
    };

    std::vector<order_gettr> cells_in_row;
    for(index_t i=0; i<cells.size(); ++i){
        cells_in_row.push_back(order_gettr(cells[i],i));
    }
    std::sort(cells_in_row.begin(), cells_in_row.end());

    std::vector<index_t> involved_nets;
    for(index_t c : cells){
        for(netlist::pin_t p : circuit.get_cell(c)){
            involved_nets.push_back(p.net_ind);
        }
    }
    // Uniquify the nets
    std::sort(involved_nets.begin(), involved_nets.end());
    involved_nets.resize(std::distance(involved_nets.begin(), std::unique(involved_nets.begin(), involved_nets.end())));

    std::vector<net_properties> net_props;

    for(index_t n : involved_nets){
        net_properties cur_net;
        cur_net.weight = circuit.get_net(n).weight; 

        for(netlist::pin_t p : circuit.get_net(n)){
            auto it = std::lower_bound(cells_in_row.begin(), cells_in_row.end(), p.cell_ind);
            if(it != cells_in_row.end() and it->cell_ind == p.cell_ind){
                // Found a pin on the row
                if(it->seq_order < cur_net.cell_fst){
                    cur_net.cell_fst = it->seq_order;
                    cur_net.offs_fst = std::pair<int_t, int_t>(p.offset.x_, p.offset.x_);
                }
                else if(it->seq_order == cur_net.cell_fst){
                    cur_net.offs_fst.first  = std::min(p.offset.x_, cur_net.offs_fst.first);
                    cur_net.offs_fst.second = std::max(p.offset.x_, cur_net.offs_fst.second);
                }
                if(it->seq_order > cur_net.cell_lst){
                    cur_net.cell_lst = it->seq_order;
                    cur_net.offs_lst = std::pair<int_t, int_t>(p.offset.x_, p.offset.x_);
                }
                else if(it->seq_order == cur_net.cell_lst){
                    cur_net.offs_lst.first  = std::min(p.offset.x_, cur_net.offs_lst.first);
                    cur_net.offs_lst.second = std::max(p.offset.x_, cur_net.offs_lst.second);
                }
            }
            else{ // Found a pin which remains fixed for this round
                cur_net.external_pins = true;

                int_t pos = pl.plt_.positions_[p.cell_ind].x_ + (pl.plt_.orientations_[p.cell_ind].x_ ? p.offset.x_ : circuit.get_cell(p.cell_ind).size.x_ - p.offset.x_);
                cur_net.ext_pos_min = std::min(pos, cur_net.ext_pos_min);
                cur_net.ext_pos_max = std::max(pos, cur_net.ext_pos_max);
            }
        }
        net_props.push_back(cur_net);
    }

    return net_props;
}

// Optimizes an ordered sequence of standard cells on the same row, returns the cost and the corresponding positions
inline std::int64_t optimize_convex_sequence(netlist const & circuit, detailed_placement const & pl, std::vector<index_t> const & cells, std::vector<int_t> & positions, int_t lower_lim, int_t upper_lim){
    struct cell_bound{
        index_t c;
        int_t pos;
        int_t slope;
        bool operator<(cell_bound const o) const{ return c < o.c; }
        cell_bound(index_t order, int_t p, int_t s) : c(order), pos(p), slope(s) {}
    };

    auto net_props = get_net_properties(circuit, pl, cells);

    std::vector<cell_bound> bounds;
    std::vector<int_t> right_slopes(cells.size(), 0);
    for(auto const cur_net : net_props){
        if(cur_net.external_pins){
            index_t fst_c = cells[cur_net.cell_fst],
                    lst_c = cells[cur_net.cell_lst];
            int_t fst_pin_offs = (pl.plt_.orientations_[fst_c].x_ ?
                cur_net.offs_fst.first : circuit.get_cell(fst_c).size.x_ - cur_net.offs_fst.second);
            int_t lst_pin_offs = (pl.plt_.orientations_[lst_c].x_ ?
                cur_net.offs_lst.second : circuit.get_cell(lst_c).size.x_ - cur_net.offs_lst.first);

            bounds.push_back(cell_bound(cur_net.cell_fst, cur_net.ext_pos_min - fst_pin_offs, cur_net.weight));
            bounds.push_back(cell_bound(cur_net.cell_lst, cur_net.ext_pos_max - lst_pin_offs, cur_net.weight));

            right_slopes[cur_net.cell_lst] += cur_net.weight;
        }
        else if(cur_net.cell_fst != cur_net.cell_lst){
            right_slopes[cur_net.cell_lst] += cur_net.weight;
            right_slopes[cur_net.cell_fst] -= cur_net.weight;
        }
    }
    std::sort(bounds.begin(), bounds.end());

    full_single_row OSRP;
    for(index_t i=0, j=0; i<cells.size(); ++i){
        index_t cur_cell_ind = cells[i];

        OSRP.push_cell(circuit.get_cell(cur_cell_ind).size.x_, lower_lim, upper_lim);
        OSRP.push_slope(right_slopes[i]);
        for(; j<bounds.size() and bounds[j].c == i; ++j){
            OSRP.push_bound(bounds[j].pos, bounds[j].slope);
        }
    }

    positions = OSRP.get_placement();

    std::int64_t cost = 0;
    for(auto const cur_net : net_props){
        index_t fst_c = cells[cur_net.cell_fst],
                lst_c = cells[cur_net.cell_lst];
        int_t fst_pin_offs = (pl.plt_.orientations_[fst_c].x_ ?
            cur_net.offs_fst.first : circuit.get_cell(fst_c).size.x_ - cur_net.offs_fst.second);
        int_t lst_pin_offs = (pl.plt_.orientations_[lst_c].x_ ?
            cur_net.offs_lst.second : circuit.get_cell(lst_c).size.x_ - cur_net.offs_lst.first);

        if(cur_net.external_pins){
            cost += (std::max(cur_net.ext_pos_max, positions[cur_net.cell_lst] + lst_pin_offs) - std::min(cur_net.ext_pos_min, positions[cur_net.cell_fst] + fst_pin_offs));
        }
        else{
            cost += (lst_pin_offs - fst_pin_offs);
        }
    }
    return cost;
}

inline void generic_swaps_global(netlist const & circuit, detailed_placement & pl, index_t row_extent, index_t cell_extent,
std::function<std::int64_t(netlist const &, detailed_placement const &, std::vector<index_t> const &)> get_nets_cost){
    for(index_t main_row = 0; main_row < pl.row_cnt(); ++main_row){

        for(index_t other_row = main_row+1; other_row <= std::min(pl.row_cnt()-1, main_row+row_extent) ; ++other_row){

            index_t first_oc = pl.get_first_cell_on_row(other_row); // The first candidate cell to be examined
            for(index_t c = pl.get_first_cell_on_row(main_row); c != null_ind; c = pl.get_next_cell_on_row(c, main_row)){
                assert(pl.cell_rows_[c] == main_row);
                if( (circuit.get_cell(c).attributes & XMovable) == 0) continue; // Don't touch fixed cells

                // Number of cells after/before the end of the cell
                index_t nb_after  = 0;
                index_t nb_before = 0;
                int_t pos_low = pl.plt_.positions_[c].x_ -   circuit.get_cell(c).size.x_,
                      pos_hgh = pl.plt_.positions_[c].x_ + 2*circuit.get_cell(c).size.x_;
                for(index_t oc=first_oc; oc != null_ind and nb_after <= row_extent; oc = pl.get_next_cell_on_row(oc, other_row)){
                    assert(pl.cell_rows_[oc] == other_row);
                    if( (circuit.get_cell(oc).attributes & XMovable) == 0) continue; // Don't touche fixed cells

                    // Count the cells which should trigger stop or shouldn't be used at the next iteration
                    if(pl.plt_.positions_[oc].x_ >= pos_hgh) ++nb_after;
                    if(pl.plt_.positions_[oc].x_ + circuit.get_cell(oc).size.x_ <= pos_low) ++ nb_before;

                    if(try_swap(circuit, pl, c, oc, get_nets_cost)){
                        std::swap(c, oc);
                        if(c == first_oc) first_oc = oc;
                    }
                }
                while(nb_before > cell_extent){
                    nb_before--;
                    first_oc = pl.get_next_cell_on_row(first_oc, other_row);
                }
            }
        }
    }
    pl.selfcheck();
}
} // End anonymous namespace

void swaps_global_HPWL(netlist const & circuit, detailed_placement & pl, index_t row_extent, index_t cell_extent){
    generic_swaps_global(circuit, pl, row_extent, cell_extent,
        [](netlist const & circuit, detailed_placement const & pl, std::vector<index_t> const & involved_nets) -> std::int64_t{
        std::int64_t sum = 0;
        for(index_t n : involved_nets){
            if(circuit.get_net(n).pin_cnt <= 1) continue;
            sum += get_HPWL_length(circuit, pl.plt_, n);
        }
        return sum;
    });
}

void swaps_global_RSMT(netlist const & circuit, detailed_placement & pl, index_t row_extent, index_t cell_extent){
    generic_swaps_global(circuit, pl, row_extent, cell_extent,
        [](netlist const & circuit, detailed_placement const & pl, std::vector<index_t> const & involved_nets) -> std::int64_t{
        std::int64_t sum = 0;
        for(index_t n : involved_nets){
            if(circuit.get_net(n).pin_cnt <= 1) continue;
            sum += get_RSMT_length(circuit, pl.plt_, n);
        }
        return sum;
    });
}

void OSRP_convex_HPWL(netlist const & circuit, detailed_placement & pl){
    for(index_t r=0; r<pl.row_cnt(); ++r){
        index_t OSRP_cell = pl.get_first_cell_on_row(r);

        while(OSRP_cell != null_ind){
            std::vector<index_t> cells;

            for(;
                OSRP_cell != null_ind and pl.cell_height(OSRP_cell) == 1 and (circuit.get_cell(OSRP_cell).attributes & XMovable) != 0; // Movable standard cell
                OSRP_cell = pl.neighbours_[pl.neighbours_limits_[OSRP_cell]].second
            ){
                cells.push_back(OSRP_cell);
            }

            if(not cells.empty()){
                // Limits of the placement region for the cells taken
                int_t lower_lim = pl.get_limit_positions(circuit, cells.front()).first,
                      upper_lim = pl.get_limit_positions(circuit, cells.back()).second;
                std::vector<int_t> final_positions;
                optimize_convex_sequence(circuit, pl, cells, final_positions, lower_lim, upper_lim);

                // Update the positions
                for(index_t i=0; i<cells.size(); ++i){
                    pl.plt_.positions_[cells[i]].x_ = final_positions[i];
                }
            }

            if(OSRP_cell != null_ind) OSRP_cell = pl.get_next_cell_on_row(OSRP_cell, r); // Go to the next group
        } // Iteration on the entire row
    } // Iteration on the rows

    pl.selfcheck();
}

void swaps_row_HPWL(netlist const & circuit, detailed_placement & pl, index_t range){
    assert(range >= 2);

    for(index_t r=0; r<pl.row_cnt(); ++r){
        index_t OSRP_cell = pl.get_first_cell_on_row(r);

        while(OSRP_cell != null_ind){
            std::vector<index_t> cells;
            for(index_t nbr_cells=0;
                    OSRP_cell != null_ind
                and pl.cell_height(OSRP_cell) == 1
                and (circuit.get_cell(OSRP_cell).attributes & XMovable) != 0
                and nbr_cells < range;
                OSRP_cell = pl.neighbours_[pl.neighbours_limits_[OSRP_cell]].second, ++nbr_cells
            ){
                cells.push_back(OSRP_cell);
            }

            if(not cells.empty()){
                // Limits of the placement region for the cells taken
                int_t lower_lim = pl.get_limit_positions(circuit, cells.front()).first,
                      upper_lim = pl.get_limit_positions(circuit, cells.back()).second;

                std::int64_t best_cost = std::numeric_limits<std::int64_t>::max();
                std::vector<int_t> positions(cells.size());
                std::vector<index_t> best_permutation = cells;
                std::vector<int_t> best_positions(cells.size());;
                std::vector<index_t> old_permutation = cells;

                // Check every possible permutation of the cells
                std::sort(cells.begin(), cells.end());
                do{
                    std::int64_t cur_cost = optimize_convex_sequence(circuit, pl, cells, positions, lower_lim, upper_lim);
                    assert(std::isfinite(cur_cost) );
                    if(cur_cost < best_cost){
                        best_cost = cur_cost;
                        best_permutation = cells;
                        best_positions = positions;
                    }
                }while(std::next_permutation(cells.begin(), cells.end()));
    
                cells = best_permutation;
                // Update the positions and the topology
                for(index_t i=0; i<cells.size(); ++i){
                    pl.plt_.positions_[cells[i]].x_ = best_positions[i];
                }

                pl.reorder_standard_cells(old_permutation, cells);
           }
    
            if(OSRP_cell != null_ind){
                // We are on a non-movable cell
                if( (circuit.get_cell(OSRP_cell).attributes & XMovable) == 0 or pl.cell_height(OSRP_cell) != 1){
                    OSRP_cell = pl.get_next_cell_on_row(OSRP_cell, r); // Go to the next group
                }
                else{ // We optimized with the maximum number of cells: just advance a few cells and optimize again
                    assert(cells.size() == range);
                    OSRP_cell = cells[range/2];
                }
            }
        } // Iteration on the entire row
    } // Iteration on the rows

    pl.selfcheck();
}

void row_compatible_orientation(netlist const & circuit, detailed_placement & pl, bool first_row_orient){
    for(index_t c=0; c<circuit.cell_cnt(); ++c){
        if( (circuit.get_cell(c).attributes & YFlippable) != 0 and pl.cell_height(c) == 1){
            pl.plt_.orientations_[c].y_ = (pl.cell_rows_[c] % 2 != 0) ^ first_row_orient;
        }
    }
}

} // namespace dp
} // namespace coloquinte


