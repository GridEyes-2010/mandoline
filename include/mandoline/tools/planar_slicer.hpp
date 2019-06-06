#pragma once

#include <mtao/types.hpp>
#include <Eigen/Geometry>
#include <mtao/geometry/grid/staggered_grid.hpp>
#include <mandoline/construction/cutdata.hpp>

namespace mandoline::tools {
    class SliceGenerator:  public mtao::geometry::grid::StaggeredGrid<double,3>{
        public:
using Base = mtao::geometry::grid::StaggeredGrid<double,3>;
            SliceGenerator() {}
            SliceGenerator(const mtao::ColVecs3d& V, const mtao::ColVecs3i& F);
            std::tuple<mtao::ColVecs3d, mtao::ColVecs3i> slice(const mtao::Vec3d& origin, const mtao::Vec3d& direction) ;
            std::tuple<mtao::ColVecs3d, mtao::ColVecs3i> slice(const Eigen::Affine3d& t) ;
            void update_embedding(const mtao::ColVecs3d& V);
            //std::tuple<mtao::ColVecs3d& V, mtao::ColVecs3i& F> slice(const mtao::ColVecs3d& V, const CutData& cut_data) const;
        private:
            mtao::ColVecs3d V;
            construction::CutData<3> data;
    };

    template <typename... Args>
    std::tuple<mtao::ColVecs3d, mtao::ColVecs3i> slice(const mtao::ColVecs3d& V, const mtao::ColVecs3i& F, Args&&... args) {
        SliceGenerator sg(V,F);
        return sg.slice(std::forward<Args>(args)...);
    }


}

