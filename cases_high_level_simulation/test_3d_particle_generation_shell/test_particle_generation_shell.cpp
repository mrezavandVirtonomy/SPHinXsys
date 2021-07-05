#include <gtest/gtest.h>
#include "solid_structural_simulation_class.h"

Real tolerance = 1e-6;

TEST(StructuralSimulation, ShellParticles)
{
	Real resolution_plate = 1.5;
	Real resolution_tube = 1.5;
	string relative_input_path = "./input/"; //path definition for linux
	string plate_stl = "plate_50_50_4.stl";
	string tube_stl = "tube_100_90_100.stl";
	vector<string> imported_stl_list = { plate_stl, tube_stl };
	vector<Vec3d> translation_list = { Vec3d(0), Vec3d(0) };
	vector<Real> resolution_list = { resolution_plate, resolution_tube };
	Real rho_0 = 1000.0;
	Real poisson = 0.35;
	Real Youngs_modulus = 1e4;
	Real physical_viscosity = 200;
	LinearElasticSolid material = LinearElasticSolid(rho_0, Youngs_modulus, poisson);
	vector<LinearElasticSolid> material_model_list = { material, material };
	StructuralSimulationInput input
	{
		relative_input_path,
		imported_stl_list,
		1.0,
		translation_list,
		resolution_list,
		material_model_list,
		physical_viscosity,
		{}
	};
	input.particle_relaxation_list_ = { false, false };

	StructuralSimulation sim(input);

	vector<ShellParticleGeneratorLattice>& particle_generator_list = sim.get_patricle_generator_list_();
	
	EXPECT_EQ(particle_generator_list[0].get_lattice_spacing_(), resolution_plate);
	EXPECT_EQ(particle_generator_list[0].get_number_of_particles_(), 100);
	
	EXPECT_EQ(particle_generator_list[1].get_lattice_spacing_(), resolution_tube);
	EXPECT_EQ(particle_generator_list[1].get_number_of_particles_(), 1194);
}


int main(int argc, char* argv[])
{	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
