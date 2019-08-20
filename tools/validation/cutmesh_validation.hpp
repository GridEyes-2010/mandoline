#include "mandoline/mesh3.hpp"

//grid cell isolation
//pcwn
// number of cells taht are pcwn {{is_pcwn,is_not_pcwn}}
std::array<int,2> pcwn_count(const mandoline::CutCellMesh<3>&);
bool pcwn_check(const mandoline::CutCellMesh<3>&);
//surface area
bool faces_fully_utilized(const mandoline::CutCellMesh<3>&);
//grid volume
bool grid_cells_fully_utilized(const mandoline::CutCellMesh<3>&);
//region counts {{mandoline regions, igl regions}}
std::array<int,2> region_counts(const mandoline::CutCellMesh<3>&);
bool equal_region_check(const mandoline::CutCellMesh<3>&);
//input volume
bool volume_check(const mandoline::CutCellMesh<3>&);

//face topological utilization
bool paired_boundary(const mandoline::CutCellMesh<3>&);


//UTILITY
//
std::array<int,2> region_counts(const mandoline::CutCellMesh<3>&, const mtao::ColVecs2i&);
mtao::ColVecs2i input_mesh_regions(const mandoline::CutCellMesh<3>&);
std::map<int,double> region_volumes(const mandoline::CutCellMesh<3>&);
mtao::VecXd brep_region_volumes(const mandoline::CutCellMesh<3>&);
//for a set of vertices that comprise a face, list the set of potential cells that it resides within
std::set<std::array<int,3>> possible_cells(const mandoline::CutCellMesh<3>& ccm, const std::vector<int>& face);
