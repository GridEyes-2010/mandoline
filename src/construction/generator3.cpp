#include "mandoline/construction/generator.hpp" 
#include <mtao/geometry/mesh/boundary_facets.h>
#include <mtao/geometry/mesh/boundary_elements.h>
#include <mtao/geometry/grid/grid_data.hpp>
#include <mtao/colvector_loop.hpp>
#include <mtao/geometry/mesh/dual_edges.hpp>
#include <mtao/eigen/stl2eigen.hpp>
#include <mtao/iterator/enumerate.hpp>
#include <mtao/logging/logger.hpp>
#include <mtao/reindexer.hpp>
#include "mandoline/construction/subgrid_transformer.hpp"
#include <variant>
#include "mandoline/construction/cell_collapser.hpp"
using namespace mtao::iterator;
using namespace mtao::logging;


namespace mandoline::construction {


    CutCellGenerator<3>::~CutCellGenerator() {}

    template <>
        CutCellMesh<3> CutCellEdgeGenerator<3>::generate() const {
            CutCellMesh<3> ccm = generate_edges();
            //make edge stuff
            return ccm;
        }

    CutCellMesh<3> CutCellGenerator<3>::generate() const {

        CutCellMesh<3> ccm = CCEG::generate();
        ccm.cut_edges = mtao::eigen::hstack(ccm.cut_edges,mtao::eigen::stl2eigen(adaptive_edges));
        ccm.m_folded_faces = folded_faces;
        //extra_metadata(ccm);
        ccm.m_faces.clear();
        ccm.m_faces.resize(faces().size());
        if(adaptive) {
            ccm.m_adaptive_grid = *adaptive_grid;
            if(adaptive_grid_regions) {
                ccm.m_adaptive_grid_regions = *adaptive_grid_regions;
            }
        }

        //ccm.triangulated_cut_faces.resize(faces().size());
        mtao::map<int,int> reindexer;
        int max = m_cut_faces.size();
        for(auto&& [i,ff]: mtao::iterator::enumerate(faces())) {
            auto&& [idx,f] = ff;
            reindexer[idx] = i;
        }
        for(auto&& [i,f]: m_faces) {
            ccm.m_faces[reindexer.at(i)] = f;
        }
        auto redx = [&](const std::set<int>& i) {
            std::set<int> ret;
            std::transform(i.begin(),i.end(),std::inserter(ret,ret.end()), [&](int idx) -> int { return reindexer.at(idx); });
            return ret;
        };
        for(auto&& mfi: mesh_face_indices) {
            int idx = reindexer.at(mfi);
            auto&& cutface = m_cut_faces[mfi];
            mtao::ColVecs3d B(3,cutface.size());
            for(auto&& [idx,i]: mtao::iterator::enumerate(cutface.indices)) {
                if(i < grid_vertex_size()) {
                    B.col(idx) = data().get_bary(cutface.parent_fid,grid_vertex(i));
                } else {
                    B.col(idx) = data().get_bary(cutface.parent_fid,crossing(i));
                }

            }
            ccm.m_mesh_faces[idx] = BarycentricTriangleFace{std::move(B),cutface.parent_fid};


        }
        //ccm.mesh_faces = redx(mesh_face_indices);
        for(auto&& [a,b]: mtao::iterator::zip(ccm.m_axial_faces,axis_face_indices)) {
            a = redx(b);
        }
        /*
           if(!performance) {
           std::cout << "Making triangulated faces" << std::endl;
           for(auto&& [i,ff]: faces_triangulated_map()) {

           if(reindexer.find(i) != reindexer.end()) {
           ccm.triangulated_cut_faces[reindexer.at(i)] = ff;
           }
           }
           }
           */

        ccm.m_cells.clear();
        ccm.m_cells.resize(cell_boundaries.size());

        for(auto&& [a,b]: mtao::iterator::zip(cell_boundaries, ccm.m_cells)) {
            b.index = a.index;
            b.region = a.region;
            std::set<int> inds;
            for(auto&& [i,j]: a) {
                int fidx = reindexer.at(i);
                b[fidx] = j;
                inds.insert(fidx);

            }
            b.grid_cell = *possible_cells_cell(inds,ccm.faces()).begin();
        }
        ccm.m_origV.resize(3,origV().size());
        for(int i = 0; i < origV().size(); ++i) {
            ccm.m_origV.col(i) = origV()[i];
        }
        ccm.m_origF = data().F();


        return ccm;
    }


    auto CutCellGenerator<3>::smallest_ordered_edge(const std::vector<int>& v) const -> Edge {
        assert(v.size()>=2);
        Edge min{{v[0],v[1]}};
        for(int i = 0; i < v.size(); ++i) {
            int j = (i+1)%v.size();
            Edge e{{v[i],v[j]}};
            min = std::min(e,min);
        }
        return min;
    } 

    void CutCellGenerator<3>::extra_metadata(CutCellMesh<3>& mesh) const {

    }







    void CutCellGenerator<3>::bake() {
        auto t2 = mtao::logging::profiler("general bake",false,"profiler");
        CCEG::bake();

        auto t = mtao::logging::profiler("grid bake cells",false,"profiler");
        bake_cells();
    }
    void CutCellGenerator<3>::bake_cells() {
        {
            auto t = mtao::logging::profiler("cell collapser",false,"profiler");
            CellCollapser cc(m_faces);
            auto V = all_GV();

            if(adaptive) {
                auto adaptive_grid_factory = AdaptiveGridFactory(m_active_grid_cell_mask);
                adaptive_grid_factory.make_cells(adaptive_level);
                adaptive_grid  = adaptive_grid_factory.create();
            }
            //auto [a,b] = adaptive_grid->compute_edges(adaptive_level);

            cc.bake(V);
            folded_faces = cc.folded_faces();
            //m_normal_faces = cc.m_faces;
            boundary_vertices.clear();
            for(int i = 0; i < StaggeredGrid::vertex_size(); ++i) {
                boundary_vertices.insert(i);
            }

            cc.remove_boundary_cells_from_vertices(boundary_vertices);
            auto cb = cc.cell_boundaries();

            cell_boundaries.resize(cb.size());
            for(auto&& [a,b]: mtao::iterator::zip(cell_boundaries,cb)) {
                a.insert(b.begin(),b.end());
            }
        }

        {
            auto&& cells = cell_boundaries;
            int max_cell_id = cells.size();
            if(adaptive) {
                //make cell names unique
                int cbsize = cells.size();
                auto& ag = *adaptive_grid;
                auto gc = ag.cells;
                ag.cells.clear();
                for(auto&& [i,c]: mtao::iterator::enumerate(gc)) {
                    auto&& [j,b] = c;
                    int id = i + cbsize;
                    ag.cells[id] = b;
                }
                max_cell_id = cbsize + ag.cells.size();
            }
            mtao::data_structures::DisjointSet<int> cell_ds;
            {
                auto t = mtao::logging::profiler("region disjoint set construction",false,"profiler");
                for(int i = 0; i < cells.size(); ++i) {
                    cell_ds.add_node(i);
                }
                for(auto&& [a,b]: m_faces) {
                    if(a >= 0) {
                        cell_ds.add_node(max_cell_id + a);
                    }
                }
                if(adaptive) {
                    auto& ag = *adaptive_grid;
                    auto grid = ag.grid();
                    auto bdry = ag.boundary(grid);
                    for(auto&& [c,b]: ag.cells) {
                        cell_ds.add_node(c);
                    }
                    for(auto&& [a,b]: bdry) {
                        if(a >= 0 && b >= 0) {
                            cell_ds.join(a,b);
                        }
                    }
                    for(auto&& [id,f]: faces()) {
                        if(f.external_boundary) {
                            auto& [cid,s] = *f.external_boundary;
                            cell_ds.join(max_cell_id + id,grid.get(cid));
                        }
                    }
                }

                for(auto [cid, faces]: mtao::iterator::enumerate(cells)) {
                    for(auto&& [fid,s]: faces) {
                        if(fid >= 0) {
                            auto&& f = m_faces[fid];
                            if(!f.is_mesh_face()) {
                                cell_ds.join(cid,fid+max_cell_id);
                            }
                        }
                    }
                }

                cell_ds.reduce_all();
            }

            int min_face_idx = 0;
            {
                int min_face_x = vertex_shape()[0];
                for(auto&& [i,f]: m_faces) {
                    if(f[0]) {
                        int x = *f[0];
                        if(x < min_face_x) {
                            min_face_idx = i;
                            min_face_x = x;
                        }
                    }
                }
            }
            /*{
              auto minf = m_faces[min_face_idx];
              std::cout << "Min facE: ";
              for(auto&& f: minf.indices) {
              std::copy(f.begin(),f.end(),std::ostream_iterator<int>(std::cout,":"));
              std::cout << " ";
              }

              std::cout << std::endl;
              }
              */
            int outside_root = -1;
            for(auto [cid, faces]: mtao::iterator::enumerate(cells)) {
                for(auto&& [fid,s]: faces) {
                    if(fid >= 0) {
                        if(fid == min_face_idx) {
                            outside_root = cell_ds.get_root(cid).data;
                            break;
                        }
                    }
                }
                if(outside_root != -1) {
                    break;
                }
            }
            std::map<int,int> reindexer;
            reindexer[outside_root] = 0;
            for(int i = 0; i < cells.size(); ++i) {
                int root = cell_ds.get_root(i).data;
                if(root != outside_root && reindexer.find(root) == reindexer.end()) {
                    reindexer[root] = reindexer.size();
                }
            }


            std::vector<int> ret(cells.size());
            std::set<int> regions;
            for(int i = 0; i < cells.size(); ++i) {
                cells[i].index = i;
                cells[i].region = reindexer[cell_ds.get_root(i).data];
                regions.insert(cells[i].region);

            }
            if(adaptive) {

                auto& ag = *adaptive_grid;
                auto& agr = *(adaptive_grid_regions = std::map<int,int>());
                for(auto&& [cid,b]: ag.cells) {
                    agr[cid] = reindexer[cell_ds.get_root(cid).data];
                }
            }


            warn() << "Region count: " << regions.size();
            auto w = warn();
            /*
            for(auto&& r: regions) {
                w << r << "(" << std::count_if(cells.begin(),cells.end(), [&](auto&& c) {
                            return c.region == r;
                            })<< ") ";
            }
            for(auto&& r: regions) {
                std::cout << r << "==========================\n";
                for(auto&& c: cells) {
                    if(c.region == r) {
                        std::cout << c.size() << ":";
                    }
                }
                std::cout << std::endl;
                for(auto&& c: cells) {
                    if(c.region == r) {
                        for(auto&& [a,b]: c) {
                            std::cout << a << std::endl;
                            std::cout << std::string(m_faces.at(a)) << std::endl;
                        }
                        std::cout << std::endl;
                    }
                }
            }
            */
        }
        mtao::logging::debug() << "Cell count: " << cell_boundaries.size();

    }
    mtao::Vec3d CutCellGenerator<3>::area_normal(const std::vector<int>& F) const {
        auto V = all_GV();
        mtao::Vec3d N = mtao::Vec3d::Zero();
        for(int i = 0; i < F.size(); ++i) {
            int j = (i+1)%F.size();
            int k = (i+2)%F.size();
            auto x = V.col(F[i]);
            auto y = V.col(F[j]);
            auto z = V.col(F[k]);
            (z-y).cross(y-x);
            N += (z-y).cross(x-y);
        }
        return N / 2;
    }
    mtao::Vec3d CutCellGenerator<3>::area_normal(const std::set<std::vector<int>>& F) const {
        mtao::Vec3d N = mtao::Vec3d::Zero();
        for(auto&& f: F) {
            N += area_normal(f);
        }
        return N;
    }

    bool CutCellGenerator<3>::check_face_utilization() const {
        auto gv = all_GV();
        std::set<int> faces;
        for(auto&& [fidx,cs]: m_faces) {
            bool do_it = true;
            for(auto&& v: cs.indices) {

                for(int i = 0; i < 3; ++i) {
                    if(cs[i]) {
                        if(axial_primal_faces[i].find(smallest_ordered_edge(v)) != axial_primal_faces[i].end()) {
                            do_it == false;
                        }
                    }
                }
            }
            if(do_it) {
                faces.insert(fidx);
            }
        }
        for(auto&& cell: cell_boundaries) {
            for(auto&& [f,s]: cell) {
                faces.erase(f);
            }
        }
        for(auto&& f: faces) {
            std::cout << "Unused face: " << f << std::endl;
            auto face = m_faces.at(f);
            std::cout << std::string(face) << std::endl;
            std::cout << std::string(face.mask()) << std::endl;
            for(auto&& f: face.indices) {
                std::copy(f.begin(),f.end(),std::ostream_iterator<int>(std::cout,":"));
                std::cout << " ";
            }
            std::cout << std::endl;
        }

        return faces.empty();

    }

    bool CutCellGenerator<3>::check_cell_containment() const {
        auto gv = all_GV();
        bool ret = true;
        for(auto&& [fidx,cs]: m_faces) {
            if(!is_in_cell(cs.indices)) {
                for(auto&& c: cs.indices) {
                    std::cout << "Bad face, lacks containment "  << std::endl;
                    for(auto&& v: c) {
                        std::string e = std::string(GV(v));
                        std::cout <<  v<< ":";
                    }
                    std::cout << std::endl;
                    for(auto&& v: c) {
                        std::string e = std::string(GV(v));
                        std::cout <<  v<<  "(" <<  gv.col(v).transpose() << ")";
                    }
                    std::cout << std::endl;
                    ret = false;
                }
            }
        }
        return ret;
    }
    auto CutCellGenerator<3>::edge_slice(int dim, int coord) const -> std::set<Edge> {
        if(auto it = axis_hem_data[dim].find(coord); it != axis_hem_data[dim].end()) {
            return it->second.edges;
        } else {
            return {};
        }
    }
}
