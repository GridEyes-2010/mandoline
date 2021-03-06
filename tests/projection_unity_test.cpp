#include <mandoline/mesh3.hpp>
#include <mtao/cmdline_parser.hpp>
#include <mtao/logging/timer.hpp>
using namespace mtao::logging;
using namespace mandoline;

//we're collecting terms from a partition of unity of the output
bool test_output_unity(const Eigen::SparseMatrix<double>& A, bool show_shape=true, bool check_coverage=true) {
    if(show_shape) {
        std::cout << A.cols() << " => " << A.rows() << std::endl;
    }
    mtao::VecXd V = A * mtao::VecXd::Ones(A.cols());
    //std::cout << V.transpose() << std::endl;
    //std::cout << "==============" << std::endl;
    //std::cout <<  (A.transpose() * mtao::VecXd::Ones(A.rows())).transpose() << std::endl;
    mtao::VecXd b = (V.array() < .5).select(V,V.array()-1);

    {
        for(int i = 0; i < V.rows(); ++i) {
            double v = b(i);
            if(std::abs(v) < 1e-5) {
            } else {
                std::cout << "bad value!" << std::endl;
                return false;
            }
        }
    }
    if(check_coverage) {
        for(int i = 0; i < A.cols(); ++i) {
            if(A.col(i).nonZeros() == 0) {
                std::cout << "Bad coverage" << i << "/" << A.cols() << std::endl;
                return false;
            }
        }
    }
    std::cout << "Works!" << std::endl;
    return true;
}
//we're splitting hte input terms
bool test_input_unity(const Eigen::SparseMatrix<double>& A) {
    std::cout << A.cols() << " => " << A.rows() << std::endl;
    return test_output_unity(A.transpose(),false,false);
}


int main(int argc, char * argv[]) {

    auto&& log = make_logger("profiler",mtao::logging::Level::All);
    mtao::CommandLineParser clp;
    clp.parse(argc, argv);

    if(clp.args().size() < 1) {
        fatal() << "No input mesh filename!";
        return {};
    }

    std::string input_cutmesh = clp.arg(0);

    CutCellMesh<3> ccm = CutCellMesh<3>::from_proto(input_cutmesh);


    //ccm.triangulate_faces();

    std::cout << "Barycentric matrix" << std::endl;
    test_output_unity(ccm.barycentric_matrix() );
    std::cout << "grid trilin matrix" << std::endl;
    test_output_unity(ccm.trilinear_matrix() );
    {

        auto A = ccm.barycentric_matrix();
        auto B = ccm.trilinear_matrix();

        auto V = ccm.vertices();
        auto OV = ccm.origV();
        auto GV = ccm.grid_vertices();

        auto NV = OV * A.transpose();
        auto NGV = GV * B.transpose();
        bool lerp_accurate = true;
        for(int i = 0; i < NV.cols(); ++i) {
            if(!ccm.is_grid_vertex(i)) {
                if((NV.col(i) - V.col(i)).norm() > 1e-5) {
                    std::cout << "Bary lerp fail " << i << ": " << NV.col(i).transpose() << " != " << ccm.vertex(i).transpose() << std::endl;
                    lerp_accurate = false;
                }
            }
        }
        if((NGV - V).norm() > 1e-7) {
            std::cout << "grid lerp fail" << std::endl;
            for(int i = 0; i < ccm.vertex_size(); ++i) {
                if((NGV.col(i) - V.col(i)).norm() > 1e-7) {
                    std::cout << NGV.col(i).transpose() << " != " << V.col(i).transpose() << std::endl;
                }
            }
            lerp_accurate = false;
        }
        if(lerp_accurate) {
            std::cout << "Projection operators were successful at lerping" << std::endl;
        }
    }

    std::cout << "face bary matrix" << std::endl;
    test_input_unity(ccm.face_barycentric_volume_matrix() );
    std::cout << "grid face matrix" << std::endl;
    test_input_unity(ccm.face_grid_volume_matrix().cwiseAbs() );

    {

        auto A = ccm.face_barycentric_volume_matrix();
        auto B = ccm.face_grid_volume_matrix();
        mtao::VecXd C = (A * mtao::VecXd::Ones(A.cols())).cwiseAbs();
        mtao::VecXd D = (B * mtao::VecXd::Ones(B.cols())).cwiseAbs();
        mtao::VecXd X = C+D;
        if((X.array() == 0).any()) {
            std::cout << "Face projection is missing out on something" << std::endl;
        } else {
            std::cout << "Face projection affects every cut-face" << std::endl;
        }
    }

    return 0;
}


