/**
 * @file 	shell_collision.cpp
 * @brief 	A rigid shell box hitting an elasric wall boundary
 * @details This is a case to test shell contact formulations in a reverse way (shell to elaticSolid).
 * @author 	Massoud Rezavand and Xiangyu Hu
 */
#include "sphinxsys.h"	//SPHinXsys Library.
using namespace SPH;	//Namespace cite here.
//----------------------------------------------------------------------
//	Basic geometry parameters and numerical setup.
//----------------------------------------------------------------------
Real DL = 4.0; 					/**< box length. */
Real DH = 4.0; 					/**< box height. */
Real resolution_ref = 0.025; 	/**< reference resolution. */
Real BW = resolution_ref * 4.; 	/**< wall width for BCs. */
BoundingBox system_domain_bounds(Vec2d(-BW, -BW), Vec2d(DL + BW, DH + BW));
Vec2d ball_center(0.25, 2.0);
Real ball_radius = 0.5;			
Real gravity_g = 1.0;
Real initial_ball_speed = 0.0;
Vec2d initial_velocity = initial_ball_speed*Vec2d(0.0, -1.);
//----------------------------------------------------------------------
//	Global paramters on material properties
//----------------------------------------------------------------------
Real rho0_s = 1.0;					/** Normalized density. */
Real Youngs_modulus = 5e4;
Real poisson = 0.45;				/** Poisson ratio. */
Real physical_viscosity = 200.0;	/** physical damping, here we choose the same value as numerical viscosity. */
//----------------------------------------------------------------------
//	Bodies with cases-dependent geometries (ComplexShape).
//----------------------------------------------------------------------
class WallBoundary : public SolidBody
{
public:
	WallBoundary(SPHSystem &sph_system, std::string body_name) 
	: SolidBody(sph_system, body_name)
	{
		std::vector<Vecd> outer_wall_shape;
		outer_wall_shape.push_back(Vecd(-BW, -BW));
		outer_wall_shape.push_back(Vecd(-BW, DH + BW));
		outer_wall_shape.push_back(Vecd(DL + BW, DH + BW));
		outer_wall_shape.push_back(Vecd(DL + BW, -BW));
		outer_wall_shape.push_back(Vecd(-BW, -BW));

		std::vector<Vecd> inner_wall_shape;
		inner_wall_shape.push_back(Vecd(0.0, 0.0));
		inner_wall_shape.push_back(Vecd(0.0, DH));
		inner_wall_shape.push_back(Vecd(DL, DH));
		inner_wall_shape.push_back(Vecd(DL, 0.0));
		inner_wall_shape.push_back(Vecd(0.0, 0.0));

		MultiPolygon multi_polygon;
		multi_polygon.addAPolygon(outer_wall_shape, ShapeBooleanOps::add);
		multi_polygon.addAPolygon(inner_wall_shape, ShapeBooleanOps::sub);
		body_shape_.add<MultiPolygonShape>(multi_polygon);
	}
};

class FreeBall : public SolidBody
{
public:
	FreeBall(SPHSystem& system, std::string body_name) : SolidBody(system, body_name)
	{
		std::vector<Vecd> outer_wall_shape;
		outer_wall_shape.push_back(Vecd(-resolution_ref, -resolution_ref) + ball_center);
		outer_wall_shape.push_back(Vecd(-resolution_ref, ball_radius + resolution_ref) + ball_center);
		outer_wall_shape.push_back(Vecd(ball_radius + resolution_ref, ball_radius + resolution_ref) + ball_center);
		outer_wall_shape.push_back(Vecd(ball_radius + resolution_ref, -resolution_ref) + ball_center);
		outer_wall_shape.push_back(Vecd(-resolution_ref, -resolution_ref) + ball_center);

		std::vector<Vecd> inner_wall_shape;
		inner_wall_shape.push_back(Vecd(0.0, 0.0) + ball_center);
		inner_wall_shape.push_back(Vecd(0.0, ball_radius) + ball_center);
		inner_wall_shape.push_back(Vecd(ball_radius, ball_radius) + ball_center);
		inner_wall_shape.push_back(Vecd(ball_radius, 0.0) + ball_center);
		inner_wall_shape.push_back(Vecd(0.0, 0.0) + ball_center);

		MultiPolygon multi_polygon;
		multi_polygon.addAPolygon(outer_wall_shape, ShapeBooleanOps::add);
		multi_polygon.addAPolygon(inner_wall_shape, ShapeBooleanOps::sub);
		body_shape_.add<MultiPolygonShape>(multi_polygon);
	}
};
/**
 * application dependent initial condition
 */
class BallInitialCondition
	: public solid_dynamics::ElasticDynamicsInitialCondition
{
public:
	BallInitialCondition(SolidBody &body)
		: solid_dynamics::ElasticDynamicsInitialCondition(body) {};
protected:
	void Update(size_t index_i, Real dt) override 
	{
		vel_n_[index_i] = initial_velocity;
	};
};
//----------------------------------------------------------------------
//	Main program starts here.
//----------------------------------------------------------------------
int main(int ac, char* av[])
{
	//----------------------------------------------------------------------
	//	Build up the environment of a SPHSystem with global controls.
	//----------------------------------------------------------------------
	SPHSystem sph_system(system_domain_bounds, resolution_ref);
	/** Tag for running particle relaxation for the initially body-fitted distribution */
	sph_system.run_particle_relaxation_ = false;
	/** Tag for starting with relaxed body-fitted particles distribution */
	sph_system.reload_particles_ = false;
	/** Tag for computation from restart files. 0: start with initial condition */
	sph_system.restart_step_ = 0;
	/** Define external force.*/
	Gravity gravity(Vecd(0.0, -gravity_g));
	/** Handle command line arguments. */
	sph_system.handleCommandlineOptions(ac, av);
	/** I/O environment. */
	In_Output 	in_output(sph_system);

	FreeBall free_ball(sph_system, "FreeBall");
	SharedPtr<ParticleGenerator> free_ball_particle_generator = makeShared<ParticleGeneratorLattice>();
	if (!sph_system.run_particle_relaxation_ && sph_system.reload_particles_)
		free_ball_particle_generator = makeShared<ParticleGeneratorReload>(in_output, free_ball.getBodyName());
	SharedPtr<NeoHookeanSolid> free_ball_material = makeShared<NeoHookeanSolid>(rho0_s, Youngs_modulus, poisson);
	ElasticSolidParticles free_ball_particles(free_ball, free_ball_material, free_ball_particle_generator);

	WallBoundary wall_boundary(sph_system, "Wall");
	SharedPtr<ParticleGenerator> wall_particle_generator = makeShared<ParticleGeneratorLattice>();
	if (!sph_system.run_particle_relaxation_ && sph_system.reload_particles_)
		wall_particle_generator = makeShared<ParticleGeneratorReload>(in_output, wall_boundary.getBodyName());
	SharedPtr<LinearElasticSolid> wall_material = makeShared<LinearElasticSolid>(rho0_s, Youngs_modulus, poisson);
	ElasticSolidParticles solid_particles(wall_boundary, wall_material, wall_particle_generator);
	//----------------------------------------------------------------------
	//	Define body relation map.
	//	The contact map gives the topological connections between the bodies.
	//	Basically the the range of bodies to build neighbor particle lists.
	//----------------------------------------------------------------------
	BodyRelationInner wall_inner(wall_boundary);
	SolidBodyRelationContact free_ball_contact(free_ball, {&wall_boundary});
	SolidBodyRelationContact wall_ball_contact(wall_boundary, {&free_ball});
	//----------------------------------------------------------------------
	//	Define the main numerical methods used in the simultion.
	//	Note that there may be data dependence on the constructors of these methods.
	//----------------------------------------------------------------------
	TimeStepInitialization wall_initialize_timestep(wall_boundary);
	solid_dynamics::CorrectConfiguration wall_corrected_configuration(wall_inner);
	solid_dynamics::AcousticTimeStepSize free_ball_get_time_step_size(wall_boundary);
	/** stress relaxation for the balls. */
	solid_dynamics::StressRelaxationFirstHalf wall_stress_relaxation_first_half(wall_inner);
	solid_dynamics::StressRelaxationSecondHalf wall_stress_relaxation_second_half(wall_inner);
	/** Algorithms for solid-solid contact. */
	solid_dynamics::ContactDensitySummation free_ball_update_contact_density(free_ball_contact);
	solid_dynamics::ShellContactDensity wall_ball_update_contact_density(wall_ball_contact);
	solid_dynamics::ContactForce free_ball_compute_solid_contact_forces(free_ball_contact);
	solid_dynamics::ContactForce wall_compute_solid_contact_forces(wall_ball_contact);
	/** initial condition */
	BallInitialCondition ball_initial_velocity(free_ball);
	//----------------------------------------------------------------------
	//	Define the methods for I/O operations and observations of the simulation.
	//----------------------------------------------------------------------
	BodyStatesRecordingToVtp	body_states_recording(in_output, sph_system.real_bodies_);
	//----------------------------------------------------------------------
	/**
	* The multi body system from simbody.
	*/
	SimTK::MultibodySystem MBsystem;
	/** The bodies or matter of the MBsystem. */
	SimTK::SimbodyMatterSubsystem matter(MBsystem);
	/** The forces of the MBsystem.*/
	SimTK::GeneralForceSubsystem forces(MBsystem);
	/** geometry and generation of the rigid body (the free ball). */
	std::vector<Vecd> outer_wall_shape;
	outer_wall_shape.push_back(Vecd(-resolution_ref, -resolution_ref) + ball_center);
	outer_wall_shape.push_back(Vecd(-resolution_ref, ball_radius + resolution_ref) + ball_center);
	outer_wall_shape.push_back(Vecd(ball_radius + resolution_ref, ball_radius + resolution_ref) + ball_center);
	outer_wall_shape.push_back(Vecd(ball_radius + resolution_ref, -resolution_ref) + ball_center);
	outer_wall_shape.push_back(Vecd(-resolution_ref, -resolution_ref) + ball_center);
	std::vector<Vecd> inner_wall_shape;
	inner_wall_shape.push_back(Vecd(0.0, 0.0) + ball_center);
	inner_wall_shape.push_back(Vecd(0.0, ball_radius) + ball_center);
	inner_wall_shape.push_back(Vecd(ball_radius, ball_radius) + ball_center);
	inner_wall_shape.push_back(Vecd(ball_radius, 0.0) + ball_center);
	inner_wall_shape.push_back(Vecd(0.0, 0.0) + ball_center);
	MultiPolygon multi_polygon;
	multi_polygon.addAPolygon(outer_wall_shape, ShapeBooleanOps::add);
	multi_polygon.addAPolygon(inner_wall_shape, ShapeBooleanOps::sub);
	MultiPolygonShape ball_multibody_shape(multi_polygon);
	SolidBodyPartForSimbody ball_multibody(free_ball, "FreeBall", ball_multibody_shape);
	/** Geometry and generation of the holder. */
	std::vector<Vecd> holder_shape;
	holder_shape.push_back(Vecd(DL, -BW));
	holder_shape.push_back(Vecd(DL, DH + BW));
	holder_shape.push_back(Vecd(DL + BW, DH + BW));
	holder_shape.push_back(Vecd(DL + BW, -BW));
	holder_shape.push_back(Vecd(DL, -BW));
	MultiPolygon multi_polygon_holder;
	multi_polygon_holder.addAPolygon(holder_shape, ShapeBooleanOps::add);
	MultiPolygonShape holder_multibody_shape(multi_polygon_holder);
	BodyRegionByParticle holder(wall_boundary, "Holder", holder_multibody_shape);
	solid_dynamics::ConstrainSolidBodyRegion	constrain_holder(wall_boundary, holder);
	/** Damping with the solid body*/
	DampingWithRandomChoice<DampingPairwiseInner<Vec2d>>
	wall_damping(wall_inner, 0.5, "Velocity", physical_viscosity);
	/** Mass properties of the rigid shell box. 
	 */
	SimTK::Body::Rigid rigid_info(*ball_multibody.body_part_mass_properties_);
	SimTK::MobilizedBody::Slider
		ballMBody(matter.Ground(), SimTK::Transform(SimTK::Vec3(0)), rigid_info, SimTK::Transform(SimTK::Vec3(0)));
	/** Gravity. */
	SimTK::Force::UniformGravity sim_gravity(forces, matter, SimTK::Vec3(Real(-150.), 0.0, 0.0));
	/** discreted forces acting on the bodies. */
	SimTK::Force::DiscreteForces force_on_bodies(forces, matter);
	/** Time steping method for multibody system.*/
	SimTK::State state = MBsystem.realizeTopology();
	SimTK::RungeKuttaMersonIntegrator integ(MBsystem);
	integ.setAccuracy(1e-3);
	integ.setAllowInterpolation(false);
	integ.initialize(state);
	/** Coupling between SimBody and SPH.*/
	solid_dynamics::TotalForceOnSolidBodyPartForSimBody
		force_on_ball(free_ball, ball_multibody, MBsystem, ballMBody, force_on_bodies, integ);
	solid_dynamics::ConstrainSolidBodyPartBySimBody
		constraint_plate(free_ball, ball_multibody, MBsystem, ballMBody, force_on_bodies, integ);
	//----------------------------------------------------------------------
	//	Prepare the simulation with cell linked list, configuration
	//	and case specified initial condition if necessary. 
	//----------------------------------------------------------------------
	sph_system.initializeSystemCellLinkedLists();
	sph_system.initializeSystemConfigurations();
	solid_particles.initializeNormalDirectionFromBodyShape();
	wall_corrected_configuration.parallel_exec();
	/** Initial states output. */
	body_states_recording.writeToFile(0);
	/** Main loop. */
	int ite 		= 0;
	Real T0 		= 10.0;
	Real End_Time 	= T0;
	Real D_Time 	= 0.01*T0;
	Real Dt 		= 0.1*D_Time;			
	Real dt 		= 0.0; 	
	//----------------------------------------------------------------------
	//	Statistics for CPU time
	//----------------------------------------------------------------------
	tick_count t1 = tick_count::now();
	tick_count::interval_t interval;
	//----------------------------------------------------------------------
	//	Main loop starts here.
	//----------------------------------------------------------------------
	while (GlobalStaticVariables::physical_time_ < End_Time)
	{
		Real integration_time = 0.0;
		while (integration_time < D_Time) 
		{
			wall_initialize_timestep.parallel_exec();
			if (ite % 100 == 0) 
			{
				std::cout << "N=" << ite << " Time: "
					<< GlobalStaticVariables::physical_time_ << "	dt: " << dt << "\n";
			}
			wall_ball_update_contact_density.parallel_exec();
			wall_compute_solid_contact_forces.parallel_exec();

			free_ball_update_contact_density.parallel_exec();
			free_ball_compute_solid_contact_forces.parallel_exec();

			{
			SimTK::State &state_for_update = integ.updAdvancedState();
			force_on_bodies.clearAllBodyForces(state_for_update);
			force_on_bodies.setOneBodyForce(state_for_update, ballMBody, force_on_ball.parallel_exec());
			integ.stepBy(dt);
			constraint_plate.parallel_exec();
			}
			
			wall_stress_relaxation_first_half.parallel_exec(dt);
			constrain_holder.parallel_exec(dt);
			wall_damping.parallel_exec(dt);
			constrain_holder.parallel_exec(dt);
			wall_stress_relaxation_second_half.parallel_exec(dt);

			free_ball.updateCellLinkedList();
			free_ball_contact.updateConfiguration();
			wall_boundary.updateCellLinkedList();
			wall_ball_contact.updateConfiguration();

			ite++;
			Real dt_free = free_ball_get_time_step_size.parallel_exec();
			dt = dt_free;
			integration_time += dt;
			GlobalStaticVariables::physical_time_ += dt;
		}
		tick_count t2 = tick_count::now();
		body_states_recording.writeToFile(ite);
		tick_count t3 = tick_count::now();
		interval += t3 - t2;
	}
	tick_count t4 = tick_count::now();

	tick_count::interval_t tt;
	tt = t4 - t1 - interval;
	std::cout << "Total wall time for computation: " << tt.seconds() << " seconds." << std::endl;
	return 0;
}
