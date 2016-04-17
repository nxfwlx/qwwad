/**
 * \file    qwwad_poisson.cpp
 * \brief   Solves Poisson equation to calculate space-charge induced potential
 * \author  Jonathan Cooper <jdc.tas@gmail.com>
 * \author  Alex Valavanis <a.valavanis@leeds.ac.uk>
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <iostream>
#include <cstdlib>
#include <valarray>

#include "qwwad/options.h"
#include "qclsim_poisson_solver.h"
#include "qwwad/constants.h"
#include "qwwad/file-io.h"

using namespace QWWAD;
using namespace constants;

/**
 * \brief Get user options
 *
 * \param[in] argc Number of command-line arguments
 * \param[in] argv Array of command-line arguments
 *
 * \return The user options
 */
Options get_options(int argc, char* argv[])
{
    Options opt;

    const std::string doc("Find the Poisson potential induced by a given charge profile");

    opt.add_option<bool>       ("uncharged",                         "True if there is no charge in the structure");
    opt.add_option<bool>       ("centred",                           "True if the potential should be pivoted "
                                                                     "around the centre of the structure");
    opt.add_option<bool>       ("mixed",                             "Use mixed boundary conditions.  By default, "
                                                                     "the space-charge effect is assumed to give "
                                                                     "zero-field boundary conditions.  By supplying "
                                                                     "this option, nonzero boundary fields can exist.");
    opt.add_option<std::string>("bandedgepotentialfile", "v_b.r",    "File containing baseline potential to be added to Poisson potential");
    opt.add_option<std::string>("dcpermittivityfile",    "eps_dc.r", "File containing the dc permittivity");
    opt.add_option<std::string>("poissonpotentialfile",  "v_p.r",    "Filename to which the Poisson potential is written.");
    opt.add_option<std::string>("totalpotentialfile",    "v.r",      "Filename to which the total potential is written.");
    opt.add_option<std::string>("chargefile",            "cd.r",     "Filename from which to read charge density profile.");
    opt.add_option<double>     ("field,E",                           "Set external electric field [kV/cm]. Only specify if "
                                                                     "the voltage drop needs to be fixed. Otherwise will be "
                                                                     "equal to inbuilt potential from zero-field Poisson solution.");
    opt.add_option<double>     ("offset",                 0 ,        "Set potential at spatial point closest to origin [meV].");
    opt.add_option<bool>       ("ptype",                             "Dopants are to be treated as acceptors, and wavefunctions "
                                                                     "treated as hole states");

    opt.add_prog_specific_options_and_parse(argc, argv, doc);

    return opt;
}

int main(int argc, char* argv[])
{
    Options opt = get_options(argc, argv);

    // Read low-frequency permittivity from file [F/m]
    std::valarray<double> z;
    std::valarray<double> _eps;
    read_table(opt.get_option<std::string>("dcpermittivityfile").c_str(), z, _eps);

    const size_t nz = z.size();

    std::valarray<double> z2(nz);
    std::valarray<double> rho(nz); // Charge-profile [C/m^2]

    // Read space-charge profile, or just leave it as zero if desired
    if(!opt.get_option<bool>("uncharged"))
        read_table(opt.get_option<std::string>("chargefile").c_str(), z2, rho);

    // Convert charge density into S.I. units
    rho *= e;

    // If we're using a p-type system, invert the charge profile so we have
    // a positive energy scale
    if(opt.get_option<bool>("ptype"))
        rho *= -1;

    const auto dz     = z[1] - z[0]; // Size of cells in sampling mesh [m] 
    const auto length = dz * nz;     // Total length of structure [m]

    // Calculate Poisson potential due to charge within structure
    std::valarray<double> phi(nz);   // Poisson potential

    // Pin the potential at the start, and make the field identical at either end
    if(opt.get_option<bool>("mixed"))
    {
        // Solve the Poisson equation with zero field at the edges first
        Poisson poisson(_eps, dz, MIXED);
        phi = poisson.solve(rho);

        // Only fix the voltage across the structure if an applied field is specified.
        // (Otherwise just return the zero-field cyclic solution!)
        if(opt.get_argument_known("field"))
        {
            // Now solve the Laplace equation to find the contribution due to applied bias
            // Find voltage drop per period and take off the voltage drop from the charge discontinuity
            // within the structure. This will ensure that the voltage drop is equal to that specified
            // rather than being the sum of applied bias and voltage due to charge which is an unknown
            // quantity.
            const auto field  = opt.get_option<double>("field") * 1000.0 * 100.0; // V/m
            const auto V_drop = field * e * length - phi[nz-1];

            if(opt.get_verbose())
                std::cout << "Voltage drop: " << V_drop / e << " V" << std::endl;

            // Instantiate Poisson class to solve the Laplace equation
            Poisson laplace(_eps, dz, DIRICHLET);
            phi += laplace.solve_laplace(V_drop);

            if(opt.get_option<bool>("centred"))
                    phi -= V_drop/2.0;
        }
    }
    else
    {
        // If a bias is specified, then pin the potential at each end
        if(opt.get_argument_known("field"))
        {
            Poisson poisson(_eps, dz, DIRICHLET);
            const auto field  = opt.get_option<double>("field") * 1000.0 * 100.0; // V/m
            const auto V_drop = field * e * length;

            if(opt.get_verbose())
                std::cout << "Voltage drop: " << V_drop / e << " V" << std::endl;

            phi = poisson.solve(rho, V_drop);

            if(opt.get_option<bool>("centred"))
            {
                // We want the potential to equal the specified value at z=0
                //   i.e., V(0) = V_drop/2
                // However, remember that the first sample location in the system is at z = dz/2
                // (i.e., in the MIDDLE of a sampling cell)
                // Therefore the potential at the first sample is V_drop/2.0 - field*e*dz/2
                phi -= (phi[0] + V_drop/2.0 - field*e*dz/2);
            }
        }
        else
        {
            Poisson poisson(_eps, dz, ZERO_FIELD);
            phi = poisson.solve(rho);
        }

        phi -= opt.get_option<double>("offset") * e/1000; // Minus offset since potential has not yet been inverted
    }

    // Invert potential as we output in electron potential instead of absolute potential.
    phi *= -1;

    // Get field profile [V/m]
    std::valarray<double> F(z.size());
    for(unsigned int iz = 1; iz < nz-1; ++iz)
        F[iz] = (phi[iz+1] - phi[iz-1])/(2*dz*e);

    write_table("field.r", z, F);
    write_table(opt.get_option<std::string>("poissonpotentialfile").c_str(), z, phi);

    // Calculate total potential and add on the baseline
    // potential if desired:
    std::valarray<double> Vtotal = phi;
    std::valarray<double> Vbase(phi.size());

    if (opt.get_argument_known("bandedgepotentialfile"))
    {
        std::valarray<double> zbase(phi.size());
        read_table(opt.get_option<std::string>("bandedgepotentialfile").c_str(), zbase, Vbase);

        // TODO: Add more robust checking of z, zbase identicality here
        if(zbase.size() != z.size())
        {
            std::ostringstream oss;
            oss << "Baseline and Poisson potential profiles have different lengths (" << zbase.size() << ") and (" << z.size() << " respectively";
            throw std::runtime_error(oss.str());
        }

        Vtotal = phi + Vbase;
    }

    write_table(opt.get_option<std::string>("totalpotentialfile").c_str(), z, Vtotal);
    
    return EXIT_SUCCESS;
}
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
