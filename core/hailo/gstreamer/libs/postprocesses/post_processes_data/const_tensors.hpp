/**
* Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
* Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
**/
#ifndef _GST_HAILO_CONST_TENSORS_HPP_
#define _GST_HAILO_CONST_TENSORS_HPP_

#include "xtensor/xarray.hpp"

xt::xarray<float> TDDFA_RESCALE_PARAMS_MEAN{3.4926363e-04, 2.5279013e-07, -6.8751979e-07, 6.0167957e+01,
                                            -6.2955132e-07, 5.7572004e-04, -5.0853912e-05, 7.4278198e+01,
                                            5.4009172e-07, 6.5741384e-05, 3.4420125e-04, -6.6671577e+01,
                                            -3.4660369e+05, -6.7468234e+04, 4.6822266e+04, -1.5262047e+04,
                                            4.3505889e+03, -5.4261453e+04, -1.8328033e+04, -1.5843289e+03,
                                            -8.4566344e+04, 3.8359607e+03, -2.0811361e+04, 3.8094930e+04,
                                            -1.9967855e+04, -9.2413701e+03, -1.9600715e+04, 1.3168090e+04,
                                            -5.2591440e+03, 1.8486478e+03, -1.3030662e+04, -2.4355562e+03,
                                            -2.2542065e+03, -1.4396562e+04, -6.1763291e+03, -2.5621920e+04,
                                            2.2639447e+02, -6.3261235e+03, -1.0867251e+04, 8.6846509e+02,
                                            -5.8311479e+03, 2.7051238e+03, -3.6294177e+03, 2.0439901e+03,
                                            -2.4466162e+03, 3.6586970e+03, -7.6459897e+03, -6.6744526e+03,
                                            1.1638839e+02, 7.1855972e+03, -1.4294868e+03, 2.6173665e+03,
                                            -1.2070955e+00, 6.6907924e-01, -1.7760828e-01, 5.6725528e-02,
                                            3.9678156e-02, -1.3586316e-01, -9.2239931e-02, -1.7260718e-01,
                                            -1.5804484e-02, -1.4168486e-01};

xt::xarray<float> TDDFA_RESCALE_PARAMS_STD{1.76321526e-04, 6.73794348e-05, 4.47084894e-04, 2.65502319e+01,
                                           1.23137695e-04, 4.49302170e-05, 7.92367064e-05, 6.98256302e+00,
                                           4.35044407e-04, 1.23148900e-04, 1.74000015e-04, 2.08030396e+01,
                                           5.75421125e+05, 2.77649062e+05, 2.58336844e+05, 2.55163125e+05,
                                           1.50994375e+05, 1.60086109e+05, 1.11277305e+05, 9.73117812e+04,
                                           1.17198453e+05, 8.93173672e+04, 8.84935547e+04, 7.22299297e+04,
                                           7.10802109e+04, 5.00139531e+04, 5.59685820e+04, 4.75255039e+04,
                                           4.95150664e+04, 3.81614805e+04, 4.48720586e+04, 4.62732383e+04,
                                           3.81167695e+04, 2.81911621e+04, 3.21914375e+04, 3.60061719e+04,
                                           3.25598926e+04, 2.55511172e+04, 2.42675098e+04, 2.75213984e+04,
                                           2.31665312e+04, 2.11015762e+04, 1.94123242e+04, 1.94522031e+04,
                                           1.74549844e+04, 2.25376230e+04, 1.61742812e+04, 1.46716406e+04,
                                           1.51156885e+04, 1.38700732e+04, 1.37463125e+04, 1.26631338e+04,
                                           1.58708346e+00, 1.50770092e+00, 5.88135779e-01, 5.88974476e-01,
                                           2.13278517e-01, 2.63020128e-01, 2.79642940e-01, 3.80302161e-01,
                                           1.61628410e-01, 2.55969286e-01};

xt::xarray<float> bfm_u_base =
    {{-7.35872031e+04, 1.85342188e+04, 1.77612930e+04, -7.16355000e+04, -1.23926746e+03, 2.01767266e+04, -6.77558594e+04,
      -1.92474238e+04, 2.19499492e+04, -6.39095586e+04, -3.54996289e+04, 2.57648320e+04, -5.84511367e+04, -5.29795078e+04,
      3.49593359e+04, -4.86543008e+04, -6.69758594e+04, 5.11847891e+04, -3.69926758e+04, -7.53740703e+04, 7.09580781e+04,
      -2.21011914e+04, -8.23799766e+04, 8.91776953e+04, -9.97121124e+01, -8.63638516e+04, 9.62798047e+04, 2.19076094e+04,
      -8.21290938e+04, 8.91085859e+04, 3.67042969e+04, -7.54103984e+04, 7.09130469e+04, 4.82796328e+04, -6.72134297e+04,
      5.12472109e+04, 5.79850000e+04, -5.34047500e+04, 3.50251992e+04, 6.34462656e+04, -3.60084414e+04, 2.58648359e+04,
      6.71454609e+04, -1.96241914e+04, 2.19214336e+04, 7.06891250e+04, -1.50561914e+03, 2.00480488e+04, 7.25602891e+04,
      1.84778066e+04, 1.79131172e+04, -5.75137656e+04, 4.03013242e+04, 8.04721797e+04, -4.92315977e+04, 4.66854883e+04,
      9.24435156e+04, -3.86623867e+04, 4.87914961e+04, 1.00783859e+05, -2.85141895e+04, 4.78889336e+04, 1.05897250e+05,
      -1.93453105e+04, 4.53389336e+04, 1.08324586e+05, 1.82724023e+04, 4.55740859e+04, 1.08260852e+05, 2.74885449e+04,
      4.81799922e+04, 1.05741305e+05, 3.76963008e+04, 4.91407070e+04, 1.00549234e+05, 4.84490430e+04, 4.70097188e+04,
      9.22795703e+04, 5.67314609e+04, 4.04576250e+04, 8.03234141e+04, -2.60905487e+02, 2.63334824e+04, 1.11411219e+05,
      -2.16724335e+02, 1.42852705e+04, 1.20398492e+05, -1.10514717e+02, 2.28539404e+03, 1.29726859e+05, -1.14867363e+02,
      -7.99934570e+03, 1.31937234e+05, -1.22622168e+04, -1.69639180e+04, 1.09804531e+05, -7.18394824e+03, -1.80073809e+04,
      1.14322297e+05, -2.83630371e+02, -1.93568770e+04, 1.16415719e+05, 6.55826123e+03, -1.79867188e+04, 1.14289453e+05,
      1.15820264e+04, -1.69011914e+04, 1.09755766e+05, -4.35319336e+04, 2.59349277e+04, 8.72518984e+04, -3.71553867e+04,
      2.97788887e+04, 9.54120547e+04, -2.80870156e+04, 2.99009375e+04, 9.55950859e+04, -1.96450605e+04, 2.56049590e+04,
      9.31052969e+04, -2.72607402e+04, 2.34987188e+04, 9.52987734e+04, -3.67323672e+04, 2.30255000e+04, 9.30262656e+04,
      1.83597578e+04, 2.56084277e+04, 9.27840156e+04, 2.68588359e+04, 3.00101426e+04, 9.51868984e+04, 3.61512500e+04,
      2.97651953e+04, 9.51682344e+04, 4.27301250e+04, 2.58199121e+04, 8.71874844e+04, 3.57689453e+04, 2.31636055e+04,
      9.29501406e+04, 2.61778516e+04, 2.34850078e+04, 9.51048281e+04, -2.57776289e+04, -4.12316797e+04, 9.92061016e+04,
      -1.67560391e+04, -3.52842891e+04, 1.10437352e+05, -5.99480566e+03, -3.13890684e+04, 1.16480539e+05, -2.64001709e+02,
      -3.25097559e+04, 1.17079453e+05, 5.44354541e+03, -3.13913379e+04, 1.16457258e+05, 1.61491680e+04, -3.52906836e+04,
      1.10337359e+05, 2.45962793e+04, -4.12682188e+04, 9.89942969e+04, 1.56132715e+04, -4.54192422e+04, 1.08561773e+05,
      7.92000928e+03, -4.83141914e+04, 1.12856117e+05, -2.39078278e+02, -4.88168750e+04, 1.13861016e+05, -8.33755957e+03,
      -4.82793867e+04, 1.13022656e+05, -1.59420254e+04, -4.54455469e+04, 1.08729125e+05, -2.32469180e+04, -4.07162812e+04,
      9.99537266e+04, -7.67756445e+03, -3.81009766e+04, 1.11375852e+05, -3.79065887e+02, -3.79130938e+04, 1.13163102e+05,
      6.97602539e+03, -3.81594766e+04, 1.11353492e+05, 2.29342090e+04, -4.07978281e+04, 9.95834531e+04, 6.79684863e+03,
      -4.05065586e+04, 1.11575406e+05, -4.01142456e+02, -4.08664961e+04, 1.12449688e+05, -7.49982080e+03, -4.04330664e+04,
      1.11496352e+05}};

#endif