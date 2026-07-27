#pragma once

namespace BlackHoleConstants {
    //===== Physics Constants =====
    constexpr double kGravitationalConstant = 6.67430e-11;  //m^3 kg^-1 s^-2 (CODATA 2018)
    constexpr double kSpeedOfLight = 2.99792458e8;          //m/s (exact)
    constexpr double kSolarMass = 1.98847e30;               //kg (IAU 2015)

    //===== Simulation Parameters =====
    constexpr double kBlackHoleMassSolarMasses = 5.0;       //Black hole mass in solar masses
    constexpr double kSimulationScale = 0.0001016;          //Meters to simulation units

    //===== Accretion Disk (in Schwarzschild radii) =====
    constexpr float kDiskInnerRadiusMultiplier = 3.0f;      //ISCO for Schwarzschild black hole
    constexpr float kDiskOuterRadiusMultiplier = 10.0f;     //Arbitrary outer boundary

    //===== Bloom Post-Processing =====
    constexpr float kBloomThreshold = 0.1f;                 //Brightness threshold (0-1)
    constexpr int kBloomBlurPasses = 8;                     //Number of ping-pong blur iterations
    constexpr float kBloomStrength = 0.0f;                  //Bloom intensity multiplier (currently disabled)

    //===== Procedural Stars =====
    constexpr float kStarDensityThreshold = 0.9993f;        //Hash threshold (~0.07% of sky)
	constexpr float kStarBrightness = 1.2f;                 //Star intensity multiplier (Both currently disabled)

    //===== Grid Visualization =====
    constexpr float kGridWellDepthMultiplier = 5.0f;        //Gravitational well depth multiplier
    constexpr float kGridMin = -50.0f;                      //Grid minimum extent
    constexpr float kGridMax = 50.0f;                       //Grid maximum extent
    constexpr float kGridSpacing = 1.0f;                    //Grid line spacing

    //===== Camera Settings =====
    constexpr float kCameraSpeedMultiplier = 4.0f;          //Speed boost when holding Shift

    //===== Time Scaling =====
    constexpr double kTimeScaleYearToMinute = 31557600.0 / 60.0; //1 year in simulation = 1 minute real time
}