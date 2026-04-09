#include <iostream>
#include <fstream>
#include <string.h>
#include <cassert>
#include <cmath>

#include "nnue.h"
#include "board.h"
#include "spsa.h"

#if defined(ARCH_ARM)
#include <arm_neon.h>
#endif

#ifdef _MSC_VER
#define SP_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#include "incbin/incbin.h"
// This will define the following variables:
// const unsigned char        gNETWORKData[];
// const unsigned char *const gNETWORKEnd;
// const unsigned int         gNETWORKSize;
INCBIN(NETWORK, EVALFILE);

#ifdef SP_MSVC
#pragma pop_macro("_MSC_VER")
#undef SP_MSVC
#endif

NetworkData* globalNetworkData;
alignas(ALIGNMENT) uint16_t nnzLookup[256][8];

float l3Weights[OUTPUT_BUCKETS][L3_SIZE + 2 * L2_SIZE];

#if defined(PROCESS_NET)
NNZ nnz;
#endif

TUNE_FLOAT(l3_0_0, 0.426452f, -1e-05f, 0.852905f);
TUNE_FLOAT(l3_0_1, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_0_2, 0.23832f, -1e-05f, 0.47664f);
TUNE_FLOAT(l3_0_3, -0.89452f, -1.78904f, 1e-05f);
TUNE_FLOAT(l3_0_4, -0.59867f, -1.19734f, 1e-05f);
TUNE_FLOAT(l3_0_5, -0.603766f, -1.20753f, 1e-05f);
TUNE_FLOAT(l3_0_6, -0.724243f, -1.44849f, 1e-05f);
TUNE_FLOAT(l3_0_7, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_0_8, -0.276749f, -0.553498f, 1e-05f);
TUNE_FLOAT(l3_0_9, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_0_10, -0.379477f, -0.758954f, 1e-05f);
TUNE_FLOAT(l3_0_11, -0.0499289f, -0.0998579f, 1e-05f);
TUNE_FLOAT(l3_0_12, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_0_13, -0.310811f, -0.621623f, 1e-05f);
TUNE_FLOAT(l3_0_14, -5.77678e-16f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_0_15, 0.132233f, -1e-05f, 0.264466f);
TUNE_FLOAT(l3_0_16, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_0_17, 0.473981f, -1e-05f, 0.947962f);
TUNE_FLOAT(l3_0_18, -0.638654f, -1.27731f, 1e-05f);
TUNE_FLOAT(l3_0_19, 0.527764f, -1e-05f, 1.05553f);
TUNE_FLOAT(l3_0_20, 3.8821e-16f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_0_21, 0.858806f, -1e-05f, 1.71761f);
TUNE_FLOAT(l3_0_22, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_0_23, 0.989857f, -1e-05f, 1.97971f);
TUNE_FLOAT(l3_0_24, -0.485326f, -0.970652f, 1e-05f);
TUNE_FLOAT(l3_0_25, 0.324651f, -1e-05f, 0.649303f);
TUNE_FLOAT(l3_0_26, 0.418677f, -1e-05f, 0.837354f);
TUNE_FLOAT(l3_0_27, -0.7256f, -1.4512f, 1e-05f);
TUNE_FLOAT(l3_0_28, 2.58556e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_0_29, 0.389557f, -1e-05f, 0.779114f);
TUNE_FLOAT(l3_0_30, 0.989951f, -1e-05f, 1.9799f);
TUNE_FLOAT(l3_0_31, -0.256726f, -0.513451f, 1e-05f);
TUNE_FLOAT(l3_0_32, 0.0651281f, -1e-05f, 0.130256f);
TUNE_FLOAT(l3_0_33, -0.0576329f, -0.115266f, 1e-05f);
TUNE_FLOAT(l3_0_34, -0.0219607f, -0.0439213f, 1e-05f);
TUNE_FLOAT(l3_0_35, 0.0689379f, -1e-05f, 0.137876f);
TUNE_FLOAT(l3_0_36, -0.0271074f, -0.0542148f, 1e-05f);
TUNE_FLOAT(l3_0_37, 0.0084744f, -1e-05f, 0.0169488f);
TUNE_FLOAT(l3_0_38, -0.0185959f, -0.0371918f, 1e-05f);
TUNE_FLOAT(l3_0_39, 0.0119571f, -1e-05f, 0.0239142f);
TUNE_FLOAT(l3_0_40, -0.0158202f, -0.0316403f, 1e-05f);
TUNE_FLOAT(l3_0_41, -0.0330924f, -0.0661849f, 1e-05f);
TUNE_FLOAT(l3_0_42, 0.0262029f, -1e-05f, 0.0524059f);
TUNE_FLOAT(l3_0_43, -0.026638f, -0.0532761f, 1e-05f);
TUNE_FLOAT(l3_0_44, 0.00917797f, -1e-05f, 0.0183559f);
TUNE_FLOAT(l3_0_45, -0.0715704f, -0.143141f, 1e-05f);
TUNE_FLOAT(l3_0_46, -0.0530438f, -0.106088f, 1e-05f);
TUNE_FLOAT(l3_0_47, 0.0138715f, -1e-05f, 0.0277431f);
TUNE_FLOAT(l3_0_48, 0.00275997f, -1e-05f, 0.00551994f);
TUNE_FLOAT(l3_0_49, 0.061881f, -1e-05f, 0.123762f);
TUNE_FLOAT(l3_0_50, -0.0150738f, -0.0301475f, 1e-05f);
TUNE_FLOAT(l3_0_51, 0.00110186f, -1e-05f, 0.00220371f);
TUNE_FLOAT(l3_0_52, 0.0128284f, -1e-05f, 0.0256569f);
TUNE_FLOAT(l3_0_53, -0.0562104f, -0.112421f, 1e-05f);
TUNE_FLOAT(l3_0_54, 0.0350646f, -1e-05f, 0.0701291f);
TUNE_FLOAT(l3_0_55, -0.0182094f, -0.0364188f, 1e-05f);
TUNE_FLOAT(l3_0_56, 0.0221515f, -1e-05f, 0.0443029f);
TUNE_FLOAT(l3_0_57, 0.0367998f, -1e-05f, 0.0735996f);
TUNE_FLOAT(l3_0_58, 0.0127858f, -1e-05f, 0.0255715f);
TUNE_FLOAT(l3_0_59, 0.0165015f, -1e-05f, 0.033003f);
TUNE_FLOAT(l3_0_60, -0.0118441f, -0.0236882f, 1e-05f);
TUNE_FLOAT(l3_0_61, 0.0013032f, -1e-05f, 0.0026064f);
TUNE_FLOAT(l3_0_62, 0.0359546f, -1e-05f, 0.0719092f);
TUNE_FLOAT(l3_0_63, -0.0108435f, -0.021687f, 1e-05f);
TUNE_FLOAT(l3_1_0, -0.989993f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_1_1, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_2, -0.375527f, -0.751055f, 1e-05f);
TUNE_FLOAT(l3_1_3, -0.917958f, -1.83592f, 1e-05f);
TUNE_FLOAT(l3_1_4, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_1_5, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_6, -0.989999f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_7, -0.969927f, -1.93985f, 1e-05f);
TUNE_FLOAT(l3_1_8, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_9, -0.989996f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_1_10, -1.37573e-14f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_1_11, 0.98999f, -1e-05f, 1.97998f);
TUNE_FLOAT(l3_1_12, -0.790757f, -1.58151f, 1e-05f);
TUNE_FLOAT(l3_1_13, -6.61297e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_1_14, 0.989986f, -1e-05f, 1.97997f);
TUNE_FLOAT(l3_1_15, -0.580178f, -1.16036f, 1e-05f);
TUNE_FLOAT(l3_1_16, -0.357119f, -0.714239f, 1e-05f);
TUNE_FLOAT(l3_1_17, -0.271494f, -0.542988f, 1e-05f);
TUNE_FLOAT(l3_1_18, 0.305739f, -1e-05f, 0.611479f);
TUNE_FLOAT(l3_1_19, -0.545531f, -1.09106f, 1e-05f);
TUNE_FLOAT(l3_1_20, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_1_21, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_1_22, -0.989993f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_1_23, -0.897852f, -1.7957f, 1e-05f);
TUNE_FLOAT(l3_1_24, -0.989998f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_25, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_1_26, 0.603021f, -1e-05f, 1.20604f);
TUNE_FLOAT(l3_1_27, -0.611268f, -1.22254f, 1e-05f);
TUNE_FLOAT(l3_1_28, 0.800132f, -1e-05f, 1.60026f);
TUNE_FLOAT(l3_1_29, -0.669764f, -1.33953f, 1e-05f);
TUNE_FLOAT(l3_1_30, -0.246581f, -0.493162f, 1e-05f);
TUNE_FLOAT(l3_1_31, -0.946207f, -1.89241f, 1e-05f);
TUNE_FLOAT(l3_1_32, 0.0718519f, -1e-05f, 0.143704f);
TUNE_FLOAT(l3_1_33, -0.0460611f, -0.0921221f, 1e-05f);
TUNE_FLOAT(l3_1_34, -0.134997f, -0.269994f, 1e-05f);
TUNE_FLOAT(l3_1_35, -0.0850522f, -0.170104f, 1e-05f);
TUNE_FLOAT(l3_1_36, -0.25989f, -0.51978f, 1e-05f);
TUNE_FLOAT(l3_1_37, 0.0645792f, -1e-05f, 0.129158f);
TUNE_FLOAT(l3_1_38, 0.441672f, -1e-05f, 0.883343f);
TUNE_FLOAT(l3_1_39, 0.184179f, -1e-05f, 0.368357f);
TUNE_FLOAT(l3_1_40, 0.0432987f, -1e-05f, 0.0865974f);
TUNE_FLOAT(l3_1_41, 0.101171f, -1e-05f, 0.202342f);
TUNE_FLOAT(l3_1_42, -0.178716f, -0.357431f, 1e-05f);
TUNE_FLOAT(l3_1_43, 0.591179f, -1e-05f, 1.18236f);
TUNE_FLOAT(l3_1_44, 0.0999381f, -1e-05f, 0.199876f);
TUNE_FLOAT(l3_1_45, -0.212264f, -0.424528f, 1e-05f);
TUNE_FLOAT(l3_1_46, 0.439643f, -1e-05f, 0.879286f);
TUNE_FLOAT(l3_1_47, 0.133485f, -1e-05f, 0.266969f);
TUNE_FLOAT(l3_1_48, 0.0741329f, -1e-05f, 0.148266f);
TUNE_FLOAT(l3_1_49, 0.096374f, -1e-05f, 0.192748f);
TUNE_FLOAT(l3_1_50, 0.153097f, -1e-05f, 0.306194f);
TUNE_FLOAT(l3_1_51, -0.0234533f, -0.0469065f, 1e-05f);
TUNE_FLOAT(l3_1_52, 0.250195f, -1e-05f, 0.500391f);
TUNE_FLOAT(l3_1_53, -0.0138602f, -0.0277204f, 1e-05f);
TUNE_FLOAT(l3_1_54, 0.0936853f, -1e-05f, 0.187371f);
TUNE_FLOAT(l3_1_55, -0.00655815f, -0.0131163f, 1e-05f);
TUNE_FLOAT(l3_1_56, 0.213941f, -1e-05f, 0.427881f);
TUNE_FLOAT(l3_1_57, 0.0108026f, -1e-05f, 0.0216052f);
TUNE_FLOAT(l3_1_58, -0.0166582f, -0.0333164f, 1e-05f);
TUNE_FLOAT(l3_1_59, 0.193939f, -1e-05f, 0.387879f);
TUNE_FLOAT(l3_1_60, 0.297802f, -1e-05f, 0.595605f);
TUNE_FLOAT(l3_1_61, 0.247516f, -1e-05f, 0.495032f);
TUNE_FLOAT(l3_1_62, 0.200033f, -1e-05f, 0.400066f);
TUNE_FLOAT(l3_1_63, -0.0763976f, -0.152795f, 1e-05f);
TUNE_FLOAT(l3_2_0, 0.989999f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_2_1, 2.34085e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_2_2, 0.98999f, -1e-05f, 1.97998f);
TUNE_FLOAT(l3_2_3, 0.431663f, -1e-05f, 0.863326f);
TUNE_FLOAT(l3_2_4, 0.989976f, -1e-05f, 1.97995f);
TUNE_FLOAT(l3_2_5, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_2_6, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_2_7, 0.499133f, -1e-05f, 0.998266f);
TUNE_FLOAT(l3_2_8, -0.989992f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_2_9, 0.989912f, -1e-05f, 1.97982f);
TUNE_FLOAT(l3_2_10, -0.235527f, -0.471055f, 1e-05f);
TUNE_FLOAT(l3_2_11, -0.989997f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_2_12, -0.989944f, -1.97989f, 1e-05f);
TUNE_FLOAT(l3_2_13, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_2_14, -0.597614f, -1.19523f, 1e-05f);
TUNE_FLOAT(l3_2_15, -0.989997f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_2_16, -0.627518f, -1.25504f, 1e-05f);
TUNE_FLOAT(l3_2_17, 0.980761f, -1e-05f, 1.96152f);
TUNE_FLOAT(l3_2_18, -0.989996f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_2_19, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_2_20, -1.0442e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_2_21, -0.395221f, -0.790441f, 1e-05f);
TUNE_FLOAT(l3_2_22, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_2_23, 2.58623e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_2_24, 0.989988f, -1e-05f, 1.97998f);
TUNE_FLOAT(l3_2_25, 0.694445f, -1e-05f, 1.38889f);
TUNE_FLOAT(l3_2_26, -0.737716f, -1.47543f, 1e-05f);
TUNE_FLOAT(l3_2_27, -3.47544e-17f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_2_28, -0.989987f, -1.97997f, 1e-05f);
TUNE_FLOAT(l3_2_29, -0.989994f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_2_30, 0.989931f, -1e-05f, 1.97986f);
TUNE_FLOAT(l3_2_31, -0.989995f, -1.97999f, 1e-05f);
TUNE_FLOAT(l3_2_32, 0.00376145f, -1e-05f, 0.0075229f);
TUNE_FLOAT(l3_2_33, -0.208722f, -0.417444f, 1e-05f);
TUNE_FLOAT(l3_2_34, -0.145228f, -0.290455f, 1e-05f);
TUNE_FLOAT(l3_2_35, -0.272078f, -0.544156f, 1e-05f);
TUNE_FLOAT(l3_2_36, 0.0619115f, -1e-05f, 0.123823f);
TUNE_FLOAT(l3_2_37, 0.31999f, -1e-05f, 0.63998f);
TUNE_FLOAT(l3_2_38, 0.126235f, -1e-05f, 0.25247f);
TUNE_FLOAT(l3_2_39, 0.618149f, -1e-05f, 1.2363f);
TUNE_FLOAT(l3_2_40, 0.176359f, -1e-05f, 0.352717f);
TUNE_FLOAT(l3_2_41, -0.0136696f, -0.0273391f, 1e-05f);
TUNE_FLOAT(l3_2_42, 0.197694f, -1e-05f, 0.395389f);
TUNE_FLOAT(l3_2_43, 0.23993f, -1e-05f, 0.479861f);
TUNE_FLOAT(l3_2_44, 0.172512f, -1e-05f, 0.345025f);
TUNE_FLOAT(l3_2_45, 0.180105f, -1e-05f, 0.360209f);
TUNE_FLOAT(l3_2_46, 0.0625316f, -1e-05f, 0.125063f);
TUNE_FLOAT(l3_2_47, -0.213883f, -0.427766f, 1e-05f);
TUNE_FLOAT(l3_2_48, 0.0149572f, -1e-05f, 0.0299145f);
TUNE_FLOAT(l3_2_49, -0.107227f, -0.214453f, 1e-05f);
TUNE_FLOAT(l3_2_50, 0.219155f, -1e-05f, 0.438309f);
TUNE_FLOAT(l3_2_51, 0.0682875f, -1e-05f, 0.136575f);
TUNE_FLOAT(l3_2_52, -0.108755f, -0.21751f, 1e-05f);
TUNE_FLOAT(l3_2_53, 0.434701f, -1e-05f, 0.869403f);
TUNE_FLOAT(l3_2_54, -0.0717742f, -0.143548f, 1e-05f);
TUNE_FLOAT(l3_2_55, -0.0101243f, -0.0202486f, 1e-05f);
TUNE_FLOAT(l3_2_56, -0.0079685f, -0.015937f, 1e-05f);
TUNE_FLOAT(l3_2_57, -0.0955434f, -0.191087f, 1e-05f);
TUNE_FLOAT(l3_2_58, -0.240752f, -0.481505f, 1e-05f);
TUNE_FLOAT(l3_2_59, 0.00353505f, -1e-05f, 0.00707009f);
TUNE_FLOAT(l3_2_60, -0.0368683f, -0.0737367f, 1e-05f);
TUNE_FLOAT(l3_2_61, 0.308841f, -1e-05f, 0.617683f);
TUNE_FLOAT(l3_2_62, 0.0126345f, -1e-05f, 0.025269f);
TUNE_FLOAT(l3_2_63, -0.0522687f, -0.104537f, 1e-05f);
TUNE_FLOAT(l3_3_0, -0.989954f, -1.97991f, 1e-05f);
TUNE_FLOAT(l3_3_1, -0.989982f, -1.97996f, 1e-05f);
TUNE_FLOAT(l3_3_2, 4.56388e-16f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_3_3, 0.989987f, -1e-05f, 1.97997f);
TUNE_FLOAT(l3_3_4, -0.989992f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_3_5, -0.392676f, -0.785352f, 1e-05f);
TUNE_FLOAT(l3_3_6, 0.684615f, -1e-05f, 1.36923f);
TUNE_FLOAT(l3_3_7, -0.758868f, -1.51774f, 1e-05f);
TUNE_FLOAT(l3_3_8, -0.949207f, -1.89841f, 1e-05f);
TUNE_FLOAT(l3_3_9, 0.989996f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_3_10, 0.989999f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_3_11, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_3_12, 0.989995f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_3_13, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_3_14, 0.989996f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_3_15, 0.331321f, -1e-05f, 0.662643f);
TUNE_FLOAT(l3_3_16, 0.539912f, -1e-05f, 1.07982f);
TUNE_FLOAT(l3_3_17, -0.989943f, -1.97989f, 1e-05f);
TUNE_FLOAT(l3_3_18, 0.628546f, -1e-05f, 1.25709f);
TUNE_FLOAT(l3_3_19, 0.98999f, -1e-05f, 1.97998f);
TUNE_FLOAT(l3_3_20, -0.299364f, -0.598727f, 1e-05f);
TUNE_FLOAT(l3_3_21, -0.989992f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_3_22, -0.989999f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_3_23, 0.940832f, -1e-05f, 1.88166f);
TUNE_FLOAT(l3_3_24, 0.989994f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_3_25, 0.668171f, -1e-05f, 1.33634f);
TUNE_FLOAT(l3_3_26, -0.989986f, -1.97997f, 1e-05f);
TUNE_FLOAT(l3_3_27, -6.74097e-16f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_3_28, -0.718714f, -1.43743f, 1e-05f);
TUNE_FLOAT(l3_3_29, -0.831388f, -1.66278f, 1e-05f);
TUNE_FLOAT(l3_3_30, 2.77129e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_3_31, 0.989987f, -1e-05f, 1.97997f);
TUNE_FLOAT(l3_3_32, 0.28563f, -1e-05f, 0.57126f);
TUNE_FLOAT(l3_3_33, -0.136695f, -0.27339f, 1e-05f);
TUNE_FLOAT(l3_3_34, 0.0389628f, -1e-05f, 0.0779256f);
TUNE_FLOAT(l3_3_35, 0.231289f, -1e-05f, 0.462578f);
TUNE_FLOAT(l3_3_36, 0.0496591f, -1e-05f, 0.0993182f);
TUNE_FLOAT(l3_3_37, -0.359962f, -0.719924f, 1e-05f);
TUNE_FLOAT(l3_3_38, 0.0243831f, -1e-05f, 0.0487662f);
TUNE_FLOAT(l3_3_39, 0.72621f, -1e-05f, 1.45242f);
TUNE_FLOAT(l3_3_40, -0.0754353f, -0.150871f, 1e-05f);
TUNE_FLOAT(l3_3_41, 0.186314f, -1e-05f, 0.372627f);
TUNE_FLOAT(l3_3_42, 0.3258f, -1e-05f, 0.6516f);
TUNE_FLOAT(l3_3_43, -0.215856f, -0.431713f, 1e-05f);
TUNE_FLOAT(l3_3_44, -0.179911f, -0.359822f, 1e-05f);
TUNE_FLOAT(l3_3_45, -0.110733f, -0.221465f, 1e-05f);
TUNE_FLOAT(l3_3_46, 0.302657f, -1e-05f, 0.605313f);
TUNE_FLOAT(l3_3_47, 0.125571f, -1e-05f, 0.251142f);
TUNE_FLOAT(l3_3_48, -0.302287f, -0.604575f, 1e-05f);
TUNE_FLOAT(l3_3_49, -0.0123542f, -0.0247085f, 1e-05f);
TUNE_FLOAT(l3_3_50, -0.133466f, -0.266933f, 1e-05f);
TUNE_FLOAT(l3_3_51, 0.0426581f, -1e-05f, 0.0853163f);
TUNE_FLOAT(l3_3_52, 0.0334731f, -1e-05f, 0.0669463f);
TUNE_FLOAT(l3_3_53, -0.133594f, -0.267189f, 1e-05f);
TUNE_FLOAT(l3_3_54, -0.311586f, -0.623173f, 1e-05f);
TUNE_FLOAT(l3_3_55, 0.0142519f, -1e-05f, 0.0285038f);
TUNE_FLOAT(l3_3_56, -0.160828f, -0.321656f, 1e-05f);
TUNE_FLOAT(l3_3_57, -0.0780602f, -0.15612f, 1e-05f);
TUNE_FLOAT(l3_3_58, 0.114686f, -1e-05f, 0.229371f);
TUNE_FLOAT(l3_3_59, -0.16094f, -0.32188f, 1e-05f);
TUNE_FLOAT(l3_3_60, -0.106143f, -0.212286f, 1e-05f);
TUNE_FLOAT(l3_3_61, -0.0447113f, -0.0894226f, 1e-05f);
TUNE_FLOAT(l3_3_62, -0.0594484f, -0.118897f, 1e-05f);
TUNE_FLOAT(l3_3_63, 0.0781202f, -1e-05f, 0.15624f);
TUNE_FLOAT(l3_4_0, -0.632239f, -1.26448f, 1e-05f);
TUNE_FLOAT(l3_4_1, -0.53429f, -1.06858f, 1e-05f);
TUNE_FLOAT(l3_4_2, -0.88947f, -1.77894f, 1e-05f);
TUNE_FLOAT(l3_4_3, -0.989992f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_4_4, 0.989982f, -1e-05f, 1.97996f);
TUNE_FLOAT(l3_4_5, -0.485827f, -0.971655f, 1e-05f);
TUNE_FLOAT(l3_4_6, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_4_7, 0.638745f, -1e-05f, 1.27749f);
TUNE_FLOAT(l3_4_8, -5.07436e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_4_9, 0.531185f, -1e-05f, 1.06237f);
TUNE_FLOAT(l3_4_10, -1.16901e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_4_11, 0.989987f, -1e-05f, 1.97997f);
TUNE_FLOAT(l3_4_12, 4.38359e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_4_13, -0.989989f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_4_14, -0.989999f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_4_15, -0.653727f, -1.30745f, 1e-05f);
TUNE_FLOAT(l3_4_16, 0.789118f, -1e-05f, 1.57824f);
TUNE_FLOAT(l3_4_17, 0.989963f, -1e-05f, 1.97993f);
TUNE_FLOAT(l3_4_18, -0.989987f, -1.97997f, 1e-05f);
TUNE_FLOAT(l3_4_19, 0.751828f, -1e-05f, 1.50366f);
TUNE_FLOAT(l3_4_20, -0.989991f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_4_21, -0.989988f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_4_22, -0.633616f, -1.26723f, 1e-05f);
TUNE_FLOAT(l3_4_23, -0.981119f, -1.96224f, 1e-05f);
TUNE_FLOAT(l3_4_24, 0.989977f, -1e-05f, 1.97995f);
TUNE_FLOAT(l3_4_25, 0.51672f, -1e-05f, 1.03344f);
TUNE_FLOAT(l3_4_26, 0.989987f, -1e-05f, 1.97997f);
TUNE_FLOAT(l3_4_27, 0.871999f, -1e-05f, 1.744f);
TUNE_FLOAT(l3_4_28, -0.975942f, -1.95188f, 1e-05f);
TUNE_FLOAT(l3_4_29, -3.09172e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_4_30, 0.989978f, -1e-05f, 1.97996f);
TUNE_FLOAT(l3_4_31, -0.16592f, -0.33184f, 1e-05f);
TUNE_FLOAT(l3_4_32, 0.00848923f, -1e-05f, 0.0169785f);
TUNE_FLOAT(l3_4_33, -0.295989f, -0.591979f, 1e-05f);
TUNE_FLOAT(l3_4_34, -0.380231f, -0.760461f, 1e-05f);
TUNE_FLOAT(l3_4_35, 0.0290272f, -1e-05f, 0.0580545f);
TUNE_FLOAT(l3_4_36, 0.12597f, -1e-05f, 0.251941f);
TUNE_FLOAT(l3_4_37, 0.0474061f, -1e-05f, 0.0948123f);
TUNE_FLOAT(l3_4_38, 0.189081f, -1e-05f, 0.378163f);
TUNE_FLOAT(l3_4_39, 0.502146f, -1e-05f, 1.00429f);
TUNE_FLOAT(l3_4_40, 0.620081f, -1e-05f, 1.24016f);
TUNE_FLOAT(l3_4_41, -0.344569f, -0.689138f, 1e-05f);
TUNE_FLOAT(l3_4_42, 0.0917078f, -1e-05f, 0.183416f);
TUNE_FLOAT(l3_4_43, 0.35295f, -1e-05f, 0.7059f);
TUNE_FLOAT(l3_4_44, 0.0280654f, -1e-05f, 0.0561308f);
TUNE_FLOAT(l3_4_45, 0.042693f, -1e-05f, 0.085386f);
TUNE_FLOAT(l3_4_46, -0.126348f, -0.252696f, 1e-05f);
TUNE_FLOAT(l3_4_47, 0.190844f, -1e-05f, 0.381687f);
TUNE_FLOAT(l3_4_48, -0.0736985f, -0.147397f, 1e-05f);
TUNE_FLOAT(l3_4_49, -0.215436f, -0.430872f, 1e-05f);
TUNE_FLOAT(l3_4_50, 0.0110631f, -1e-05f, 0.0221262f);
TUNE_FLOAT(l3_4_51, -0.0584967f, -0.116993f, 1e-05f);
TUNE_FLOAT(l3_4_52, 0.00686351f, -1e-05f, 0.013727f);
TUNE_FLOAT(l3_4_53, -0.30835f, -0.6167f, 1e-05f);
TUNE_FLOAT(l3_4_54, 0.195621f, -1e-05f, 0.391243f);
TUNE_FLOAT(l3_4_55, 0.00508948f, -1e-05f, 0.010179f);
TUNE_FLOAT(l3_4_56, 0.0626342f, -1e-05f, 0.125268f);
TUNE_FLOAT(l3_4_57, -0.30337f, -0.60674f, 1e-05f);
TUNE_FLOAT(l3_4_58, -0.192116f, -0.384233f, 1e-05f);
TUNE_FLOAT(l3_4_59, 0.309504f, -1e-05f, 0.619008f);
TUNE_FLOAT(l3_4_60, 0.570426f, -1e-05f, 1.14085f);
TUNE_FLOAT(l3_4_61, -0.0101311f, -0.0202622f, 1e-05f);
TUNE_FLOAT(l3_4_62, 0.178722f, -1e-05f, 0.357445f);
TUNE_FLOAT(l3_4_63, 0.154798f, -1e-05f, 0.309595f);
TUNE_FLOAT(l3_5_0, 0.989997f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_5_1, 0.605884f, -1e-05f, 1.21177f);
TUNE_FLOAT(l3_5_2, 0.989979f, -1e-05f, 1.97996f);
TUNE_FLOAT(l3_5_3, 0.714679f, -1e-05f, 1.42936f);
TUNE_FLOAT(l3_5_4, -0.835028f, -1.67006f, 1e-05f);
TUNE_FLOAT(l3_5_5, 0.612659f, -1e-05f, 1.22532f);
TUNE_FLOAT(l3_5_6, -0.738474f, -1.47695f, 1e-05f);
TUNE_FLOAT(l3_5_7, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_5_8, -0.98998f, -1.97996f, 1e-05f);
TUNE_FLOAT(l3_5_9, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_5_10, -0.737888f, -1.47578f, 1e-05f);
TUNE_FLOAT(l3_5_11, 0.715169f, -1e-05f, 1.43034f);
TUNE_FLOAT(l3_5_12, -0.80537f, -1.61074f, 1e-05f);
TUNE_FLOAT(l3_5_13, 0.499734f, -1e-05f, 0.999468f);
TUNE_FLOAT(l3_5_14, -0.8579f, -1.7158f, 1e-05f);
TUNE_FLOAT(l3_5_15, -0.99f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_5_16, -0.873423f, -1.74685f, 1e-05f);
TUNE_FLOAT(l3_5_17, 0.81646f, -1e-05f, 1.63292f);
TUNE_FLOAT(l3_5_18, -0.989987f, -1.97997f, 1e-05f);
TUNE_FLOAT(l3_5_19, 0.671895f, -1e-05f, 1.34379f);
TUNE_FLOAT(l3_5_20, 0.63658f, -1e-05f, 1.27316f);
TUNE_FLOAT(l3_5_21, 0.572772f, -1e-05f, 1.14554f);
TUNE_FLOAT(l3_5_22, 0.989995f, -1e-05f, 1.97999f);
TUNE_FLOAT(l3_5_23, -0.844217f, -1.68843f, 1e-05f);
TUNE_FLOAT(l3_5_24, 0.29066f, -1e-05f, 0.581319f);
TUNE_FLOAT(l3_5_25, 0.805454f, -1e-05f, 1.61091f);
TUNE_FLOAT(l3_5_26, 0.411145f, -1e-05f, 0.822289f);
TUNE_FLOAT(l3_5_27, 3.50704e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_5_28, 0.838224f, -1e-05f, 1.67645f);
TUNE_FLOAT(l3_5_29, 0.989988f, -1e-05f, 1.97998f);
TUNE_FLOAT(l3_5_30, -0.637673f, -1.27535f, 1e-05f);
TUNE_FLOAT(l3_5_31, 0.864321f, -1e-05f, 1.72864f);
TUNE_FLOAT(l3_5_32, 0.154909f, -1e-05f, 0.309818f);
TUNE_FLOAT(l3_5_33, -0.342558f, -0.685115f, 1e-05f);
TUNE_FLOAT(l3_5_34, -0.0108741f, -0.0217481f, 1e-05f);
TUNE_FLOAT(l3_5_35, 0.512743f, -1e-05f, 1.02549f);
TUNE_FLOAT(l3_5_36, 0.329322f, -1e-05f, 0.658644f);
TUNE_FLOAT(l3_5_37, -0.0705663f, -0.141133f, 1e-05f);
TUNE_FLOAT(l3_5_38, 0.211954f, -1e-05f, 0.423907f);
TUNE_FLOAT(l3_5_39, -0.24222f, -0.48444f, 1e-05f);
TUNE_FLOAT(l3_5_40, 0.0970267f, -1e-05f, 0.194053f);
TUNE_FLOAT(l3_5_41, 0.114452f, -1e-05f, 0.228904f);
TUNE_FLOAT(l3_5_42, 0.0561971f, -1e-05f, 0.112394f);
TUNE_FLOAT(l3_5_43, 0.217681f, -1e-05f, 0.435361f);
TUNE_FLOAT(l3_5_44, 0.0165586f, -1e-05f, 0.0331172f);
TUNE_FLOAT(l3_5_45, -0.0838479f, -0.167696f, 1e-05f);
TUNE_FLOAT(l3_5_46, 0.0362114f, -1e-05f, 0.0724227f);
TUNE_FLOAT(l3_5_47, -0.989977f, -1.97995f, 1e-05f);
TUNE_FLOAT(l3_5_48, -0.0519122f, -0.103824f, 1e-05f);
TUNE_FLOAT(l3_5_49, -0.294728f, -0.589456f, 1e-05f);
TUNE_FLOAT(l3_5_50, -0.117535f, -0.235071f, 1e-05f);
TUNE_FLOAT(l3_5_51, -0.0483008f, -0.0966016f, 1e-05f);
TUNE_FLOAT(l3_5_52, -0.0367152f, -0.0734305f, 1e-05f);
TUNE_FLOAT(l3_5_53, -0.436672f, -0.873345f, 1e-05f);
TUNE_FLOAT(l3_5_54, -0.158306f, -0.316613f, 1e-05f);
TUNE_FLOAT(l3_5_55, -0.206124f, -0.412247f, 1e-05f);
TUNE_FLOAT(l3_5_56, -0.0221682f, -0.0443363f, 1e-05f);
TUNE_FLOAT(l3_5_57, -0.0128432f, -0.0256863f, 1e-05f);
TUNE_FLOAT(l3_5_58, -0.0470128f, -0.0940255f, 1e-05f);
TUNE_FLOAT(l3_5_59, -0.154269f, -0.308537f, 1e-05f);
TUNE_FLOAT(l3_5_60, 0.0961026f, -1e-05f, 0.192205f);
TUNE_FLOAT(l3_5_61, -0.206004f, -0.412008f, 1e-05f);
TUNE_FLOAT(l3_5_62, -0.110932f, -0.221864f, 1e-05f);
TUNE_FLOAT(l3_5_63, 0.139633f, -1e-05f, 0.279267f);
TUNE_FLOAT(l3_6_0, -0.593396f, -1.18679f, 1e-05f);
TUNE_FLOAT(l3_6_1, -0.872568f, -1.74514f, 1e-05f);
TUNE_FLOAT(l3_6_2, -0.509521f, -1.01904f, 1e-05f);
TUNE_FLOAT(l3_6_3, -0.989998f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_6_4, 0.989998f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_6_5, -0.457601f, -0.915201f, 1e-05f);
TUNE_FLOAT(l3_6_6, -0.568086f, -1.13617f, 1e-05f);
TUNE_FLOAT(l3_6_7, -0.431514f, -0.863028f, 1e-05f);
TUNE_FLOAT(l3_6_8, -5.945e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_6_9, 0.694089f, -1e-05f, 1.38818f);
TUNE_FLOAT(l3_6_10, 0.623784f, -1e-05f, 1.24757f);
TUNE_FLOAT(l3_6_11, 0.218429f, -1e-05f, 0.436857f);
TUNE_FLOAT(l3_6_12, -0.626351f, -1.2527f, 1e-05f);
TUNE_FLOAT(l3_6_13, -0.989998f, -1.98f, 1e-05f);
TUNE_FLOAT(l3_6_14, -1.07442e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_6_15, -1.67394e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_6_16, -0.506295f, -1.01259f, 1e-05f);
TUNE_FLOAT(l3_6_17, -0.616383f, -1.23277f, 1e-05f);
TUNE_FLOAT(l3_6_18, -0.841207f, -1.68241f, 1e-05f);
TUNE_FLOAT(l3_6_19, -0.959754f, -1.91951f, 1e-05f);
TUNE_FLOAT(l3_6_20, 0.794201f, -1e-05f, 1.5884f);
TUNE_FLOAT(l3_6_21, 0.626258f, -1e-05f, 1.25252f);
TUNE_FLOAT(l3_6_22, 0.645591f, -1e-05f, 1.29118f);
TUNE_FLOAT(l3_6_23, -0.608918f, -1.21784f, 1e-05f);
TUNE_FLOAT(l3_6_24, 0.755558f, -1e-05f, 1.51112f);
TUNE_FLOAT(l3_6_25, 0.589794f, -1e-05f, 1.17959f);
TUNE_FLOAT(l3_6_26, -0.717416f, -1.43483f, 1e-05f);
TUNE_FLOAT(l3_6_27, 0.645876f, -1e-05f, 1.29175f);
TUNE_FLOAT(l3_6_28, -0.821538f, -1.64308f, 1e-05f);
TUNE_FLOAT(l3_6_29, -0.733885f, -1.46777f, 1e-05f);
TUNE_FLOAT(l3_6_30, 0.99f, -1e-05f, 1.98f);
TUNE_FLOAT(l3_6_31, 0.711336f, -1e-05f, 1.42267f);
TUNE_FLOAT(l3_6_32, 0.00207694f, -1e-05f, 0.00415388f);
TUNE_FLOAT(l3_6_33, 0.757823f, -1e-05f, 1.51565f);
TUNE_FLOAT(l3_6_34, -0.333239f, -0.666478f, 1e-05f);
TUNE_FLOAT(l3_6_35, -0.0173707f, -0.0347414f, 1e-05f);
TUNE_FLOAT(l3_6_36, -0.239897f, -0.479793f, 1e-05f);
TUNE_FLOAT(l3_6_37, 0.210877f, -1e-05f, 0.421753f);
TUNE_FLOAT(l3_6_38, -0.196397f, -0.392795f, 1e-05f);
TUNE_FLOAT(l3_6_39, -0.492321f, -0.984641f, 1e-05f);
TUNE_FLOAT(l3_6_40, -0.492231f, -0.984462f, 1e-05f);
TUNE_FLOAT(l3_6_41, -0.160507f, -0.321015f, 1e-05f);
TUNE_FLOAT(l3_6_42, 0.0614338f, -1e-05f, 0.122868f);
TUNE_FLOAT(l3_6_43, 0.164222f, -1e-05f, 0.328443f);
TUNE_FLOAT(l3_6_44, 0.118236f, -1e-05f, 0.236472f);
TUNE_FLOAT(l3_6_45, 0.147978f, -1e-05f, 0.295957f);
TUNE_FLOAT(l3_6_46, -0.596443f, -1.19289f, 1e-05f);
TUNE_FLOAT(l3_6_47, 0.217298f, -1e-05f, 0.434596f);
TUNE_FLOAT(l3_6_48, 0.0646816f, -1e-05f, 0.129363f);
TUNE_FLOAT(l3_6_49, -0.0231547f, -0.0463094f, 1e-05f);
TUNE_FLOAT(l3_6_50, -0.112764f, -0.225529f, 1e-05f);
TUNE_FLOAT(l3_6_51, -0.198378f, -0.396756f, 1e-05f);
TUNE_FLOAT(l3_6_52, -0.00556354f, -0.0111271f, 1e-05f);
TUNE_FLOAT(l3_6_53, 0.241747f, -1e-05f, 0.483495f);
TUNE_FLOAT(l3_6_54, -0.00978447f, -0.0195689f, 1e-05f);
TUNE_FLOAT(l3_6_55, 0.11242f, -1e-05f, 0.224841f);
TUNE_FLOAT(l3_6_56, 0.259498f, -1e-05f, 0.518996f);
TUNE_FLOAT(l3_6_57, -0.115075f, -0.23015f, 1e-05f);
TUNE_FLOAT(l3_6_58, 0.170927f, -1e-05f, 0.341853f);
TUNE_FLOAT(l3_6_59, 0.0444726f, -1e-05f, 0.0889452f);
TUNE_FLOAT(l3_6_60, -0.0467371f, -0.0934743f, 1e-05f);
TUNE_FLOAT(l3_6_61, -0.0496619f, -0.0993238f, 1e-05f);
TUNE_FLOAT(l3_6_62, 0.111471f, -1e-05f, 0.222942f);
TUNE_FLOAT(l3_6_63, 0.122491f, -1e-05f, 0.244983f);
TUNE_FLOAT(l3_7_0, -0.440445f, -0.88089f, 1e-05f);
TUNE_FLOAT(l3_7_1, 0.989958f, -1e-05f, 1.97992f);
TUNE_FLOAT(l3_7_2, -0.475146f, -0.950293f, 1e-05f);
TUNE_FLOAT(l3_7_3, 0.989923f, -1e-05f, 1.97985f);
TUNE_FLOAT(l3_7_4, -0.743292f, -1.48658f, 1e-05f);
TUNE_FLOAT(l3_7_5, 0.140897f, -1e-05f, 0.281795f);
TUNE_FLOAT(l3_7_6, 0.877635f, -1e-05f, 1.75527f);
TUNE_FLOAT(l3_7_7, 1.97891e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_8, -0.62505f, -1.2501f, 1e-05f);
TUNE_FLOAT(l3_7_9, -0.314286f, -0.628572f, 1e-05f);
TUNE_FLOAT(l3_7_10, 0.626881f, -1e-05f, 1.25376f);
TUNE_FLOAT(l3_7_11, 0.106719f, -1e-05f, 0.213437f);
TUNE_FLOAT(l3_7_12, -0.934093f, -1.86819f, 1e-05f);
TUNE_FLOAT(l3_7_13, -0.402951f, -0.805901f, 1e-05f);
TUNE_FLOAT(l3_7_14, -2.69754e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_15, 0.543288f, -1e-05f, 1.08658f);
TUNE_FLOAT(l3_7_16, 0.931058f, -1e-05f, 1.86212f);
TUNE_FLOAT(l3_7_17, -3.25025e-16f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_18, -0.98999f, -1.97998f, 1e-05f);
TUNE_FLOAT(l3_7_19, 0.737469f, -1e-05f, 1.47494f);
TUNE_FLOAT(l3_7_20, -0.585749f, -1.1715f, 1e-05f);
TUNE_FLOAT(l3_7_21, -0.644063f, -1.28813f, 1e-05f);
TUNE_FLOAT(l3_7_22, 5.39697e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_23, -6.73662e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_24, -0.657609f, -1.31522f, 1e-05f);
TUNE_FLOAT(l3_7_25, 3.23428e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_26, -0.489953f, -0.979906f, 1e-05f);
TUNE_FLOAT(l3_7_27, -1.08402e-15f, -1e-05f, 1e-05f);
TUNE_FLOAT(l3_7_28, -0.915414f, -1.83083f, 1e-05f);
TUNE_FLOAT(l3_7_29, -0.817775f, -1.63555f, 1e-05f);
TUNE_FLOAT(l3_7_30, -0.281019f, -0.562037f, 1e-05f);
TUNE_FLOAT(l3_7_31, -0.326221f, -0.652443f, 1e-05f);
TUNE_FLOAT(l3_7_32, 0.0621297f, -1e-05f, 0.124259f);
TUNE_FLOAT(l3_7_33, 0.0210404f, -1e-05f, 0.0420807f);
TUNE_FLOAT(l3_7_34, -0.165129f, -0.330258f, 1e-05f);
TUNE_FLOAT(l3_7_35, 0.639177f, -1e-05f, 1.27835f);
TUNE_FLOAT(l3_7_36, -0.205331f, -0.410662f, 1e-05f);
TUNE_FLOAT(l3_7_37, -0.391199f, -0.782398f, 1e-05f);
TUNE_FLOAT(l3_7_38, -0.157438f, -0.314876f, 1e-05f);
TUNE_FLOAT(l3_7_39, 0.015325f, -1e-05f, 0.0306499f);
TUNE_FLOAT(l3_7_40, 0.201508f, -1e-05f, 0.403015f);
TUNE_FLOAT(l3_7_41, 0.139083f, -1e-05f, 0.278165f);
TUNE_FLOAT(l3_7_42, 0.0211497f, -1e-05f, 0.0422994f);
TUNE_FLOAT(l3_7_43, -0.4023f, -0.8046f, 1e-05f);
TUNE_FLOAT(l3_7_44, -0.0323715f, -0.064743f, 1e-05f);
TUNE_FLOAT(l3_7_45, -0.0337706f, -0.0675413f, 1e-05f);
TUNE_FLOAT(l3_7_46, -0.00727222f, -0.0145444f, 1e-05f);
TUNE_FLOAT(l3_7_47, 0.119701f, -1e-05f, 0.239402f);
TUNE_FLOAT(l3_7_48, 0.442406f, -1e-05f, 0.884813f);
TUNE_FLOAT(l3_7_49, 0.126654f, -1e-05f, 0.253308f);
TUNE_FLOAT(l3_7_50, -0.0616807f, -0.123361f, 1e-05f);
TUNE_FLOAT(l3_7_51, -0.256898f, -0.513796f, 1e-05f);
TUNE_FLOAT(l3_7_52, -0.142444f, -0.284887f, 1e-05f);
TUNE_FLOAT(l3_7_53, 0.33736f, -1e-05f, 0.67472f);
TUNE_FLOAT(l3_7_54, 0.0146921f, -1e-05f, 0.0293841f);
TUNE_FLOAT(l3_7_55, -0.300428f, -0.600855f, 1e-05f);
TUNE_FLOAT(l3_7_56, 0.147867f, -1e-05f, 0.295735f);
TUNE_FLOAT(l3_7_57, 0.125388f, -1e-05f, 0.250776f);
TUNE_FLOAT(l3_7_58, 0.471079f, -1e-05f, 0.942158f);
TUNE_FLOAT(l3_7_59, 0.226108f, -1e-05f, 0.452216f);
TUNE_FLOAT(l3_7_60, 0.0222792f, -1e-05f, 0.0445583f);
TUNE_FLOAT(l3_7_61, 0.0984109f, -1e-05f, 0.196822f);
TUNE_FLOAT(l3_7_62, -0.419976f, -0.839951f, 1e-05f);
TUNE_FLOAT(l3_7_63, -0.0649789f, -0.129958f, 1e-05f);

void initNetworkData() {
    ThreatInputs::initialise();

    for (size_t i = 0; i < 256; i++) {
        uint64_t j = i;
        uint64_t k = 0;
        while (j) {
            nnzLookup[i][k++] = popLSB(&j);
        }
    }

    globalNetworkData = (NetworkData*)gNETWORKData;

    for (int i = 0; i < OUTPUT_BUCKETS; i++) {
        for (int j = 0; j < L3_SIZE + 2 * L2_SIZE; j++) {
            l3Weights[i][j] = globalNetworkData->l3Weights[i][j];

            // float minMin = -1e-5;
            // float minMax = 1e-5;

            // if (l3Weights[i][j] >= 0.0f)
            //     std::cout << "TUNE_FLOAT(l3_" << i << "_" << j << ", " << l3Weights[i][j] << "f, " << minMin << "f, " << std::max(minMax, (2 * l3Weights[i][j])) << "f);" << std::endl;
            // else
            //     std::cout << "TUNE_FLOAT(l3_" << i << "_" << j << ", " << l3Weights[i][j] << "f, " << std::min(minMin, (2 * l3Weights[i][j])) << "f, " << minMax << "f);" << std::endl;
            // std::cout << "l3Weights[" << i << "][" << j << "] = l3_" << i << "_" << j << ";" << std::endl;
        }
    }
}

void NNUE::reset(Board* board) {

    l3Weights[0][0] = l3_0_0;
    l3Weights[0][1] = l3_0_1;
    l3Weights[0][2] = l3_0_2;
    l3Weights[0][3] = l3_0_3;
    l3Weights[0][4] = l3_0_4;
    l3Weights[0][5] = l3_0_5;
    l3Weights[0][6] = l3_0_6;
    l3Weights[0][7] = l3_0_7;
    l3Weights[0][8] = l3_0_8;
    l3Weights[0][9] = l3_0_9;
    l3Weights[0][10] = l3_0_10;
    l3Weights[0][11] = l3_0_11;
    l3Weights[0][12] = l3_0_12;
    l3Weights[0][13] = l3_0_13;
    l3Weights[0][14] = l3_0_14;
    l3Weights[0][15] = l3_0_15;
    l3Weights[0][16] = l3_0_16;
    l3Weights[0][17] = l3_0_17;
    l3Weights[0][18] = l3_0_18;
    l3Weights[0][19] = l3_0_19;
    l3Weights[0][20] = l3_0_20;
    l3Weights[0][21] = l3_0_21;
    l3Weights[0][22] = l3_0_22;
    l3Weights[0][23] = l3_0_23;
    l3Weights[0][24] = l3_0_24;
    l3Weights[0][25] = l3_0_25;
    l3Weights[0][26] = l3_0_26;
    l3Weights[0][27] = l3_0_27;
    l3Weights[0][28] = l3_0_28;
    l3Weights[0][29] = l3_0_29;
    l3Weights[0][30] = l3_0_30;
    l3Weights[0][31] = l3_0_31;
    l3Weights[0][32] = l3_0_32;
    l3Weights[0][33] = l3_0_33;
    l3Weights[0][34] = l3_0_34;
    l3Weights[0][35] = l3_0_35;
    l3Weights[0][36] = l3_0_36;
    l3Weights[0][37] = l3_0_37;
    l3Weights[0][38] = l3_0_38;
    l3Weights[0][39] = l3_0_39;
    l3Weights[0][40] = l3_0_40;
    l3Weights[0][41] = l3_0_41;
    l3Weights[0][42] = l3_0_42;
    l3Weights[0][43] = l3_0_43;
    l3Weights[0][44] = l3_0_44;
    l3Weights[0][45] = l3_0_45;
    l3Weights[0][46] = l3_0_46;
    l3Weights[0][47] = l3_0_47;
    l3Weights[0][48] = l3_0_48;
    l3Weights[0][49] = l3_0_49;
    l3Weights[0][50] = l3_0_50;
    l3Weights[0][51] = l3_0_51;
    l3Weights[0][52] = l3_0_52;
    l3Weights[0][53] = l3_0_53;
    l3Weights[0][54] = l3_0_54;
    l3Weights[0][55] = l3_0_55;
    l3Weights[0][56] = l3_0_56;
    l3Weights[0][57] = l3_0_57;
    l3Weights[0][58] = l3_0_58;
    l3Weights[0][59] = l3_0_59;
    l3Weights[0][60] = l3_0_60;
    l3Weights[0][61] = l3_0_61;
    l3Weights[0][62] = l3_0_62;
    l3Weights[0][63] = l3_0_63;
    l3Weights[1][0] = l3_1_0;
    l3Weights[1][1] = l3_1_1;
    l3Weights[1][2] = l3_1_2;
    l3Weights[1][3] = l3_1_3;
    l3Weights[1][4] = l3_1_4;
    l3Weights[1][5] = l3_1_5;
    l3Weights[1][6] = l3_1_6;
    l3Weights[1][7] = l3_1_7;
    l3Weights[1][8] = l3_1_8;
    l3Weights[1][9] = l3_1_9;
    l3Weights[1][10] = l3_1_10;
    l3Weights[1][11] = l3_1_11;
    l3Weights[1][12] = l3_1_12;
    l3Weights[1][13] = l3_1_13;
    l3Weights[1][14] = l3_1_14;
    l3Weights[1][15] = l3_1_15;
    l3Weights[1][16] = l3_1_16;
    l3Weights[1][17] = l3_1_17;
    l3Weights[1][18] = l3_1_18;
    l3Weights[1][19] = l3_1_19;
    l3Weights[1][20] = l3_1_20;
    l3Weights[1][21] = l3_1_21;
    l3Weights[1][22] = l3_1_22;
    l3Weights[1][23] = l3_1_23;
    l3Weights[1][24] = l3_1_24;
    l3Weights[1][25] = l3_1_25;
    l3Weights[1][26] = l3_1_26;
    l3Weights[1][27] = l3_1_27;
    l3Weights[1][28] = l3_1_28;
    l3Weights[1][29] = l3_1_29;
    l3Weights[1][30] = l3_1_30;
    l3Weights[1][31] = l3_1_31;
    l3Weights[1][32] = l3_1_32;
    l3Weights[1][33] = l3_1_33;
    l3Weights[1][34] = l3_1_34;
    l3Weights[1][35] = l3_1_35;
    l3Weights[1][36] = l3_1_36;
    l3Weights[1][37] = l3_1_37;
    l3Weights[1][38] = l3_1_38;
    l3Weights[1][39] = l3_1_39;
    l3Weights[1][40] = l3_1_40;
    l3Weights[1][41] = l3_1_41;
    l3Weights[1][42] = l3_1_42;
    l3Weights[1][43] = l3_1_43;
    l3Weights[1][44] = l3_1_44;
    l3Weights[1][45] = l3_1_45;
    l3Weights[1][46] = l3_1_46;
    l3Weights[1][47] = l3_1_47;
    l3Weights[1][48] = l3_1_48;
    l3Weights[1][49] = l3_1_49;
    l3Weights[1][50] = l3_1_50;
    l3Weights[1][51] = l3_1_51;
    l3Weights[1][52] = l3_1_52;
    l3Weights[1][53] = l3_1_53;
    l3Weights[1][54] = l3_1_54;
    l3Weights[1][55] = l3_1_55;
    l3Weights[1][56] = l3_1_56;
    l3Weights[1][57] = l3_1_57;
    l3Weights[1][58] = l3_1_58;
    l3Weights[1][59] = l3_1_59;
    l3Weights[1][60] = l3_1_60;
    l3Weights[1][61] = l3_1_61;
    l3Weights[1][62] = l3_1_62;
    l3Weights[1][63] = l3_1_63;
    l3Weights[2][0] = l3_2_0;
    l3Weights[2][1] = l3_2_1;
    l3Weights[2][2] = l3_2_2;
    l3Weights[2][3] = l3_2_3;
    l3Weights[2][4] = l3_2_4;
    l3Weights[2][5] = l3_2_5;
    l3Weights[2][6] = l3_2_6;
    l3Weights[2][7] = l3_2_7;
    l3Weights[2][8] = l3_2_8;
    l3Weights[2][9] = l3_2_9;
    l3Weights[2][10] = l3_2_10;
    l3Weights[2][11] = l3_2_11;
    l3Weights[2][12] = l3_2_12;
    l3Weights[2][13] = l3_2_13;
    l3Weights[2][14] = l3_2_14;
    l3Weights[2][15] = l3_2_15;
    l3Weights[2][16] = l3_2_16;
    l3Weights[2][17] = l3_2_17;
    l3Weights[2][18] = l3_2_18;
    l3Weights[2][19] = l3_2_19;
    l3Weights[2][20] = l3_2_20;
    l3Weights[2][21] = l3_2_21;
    l3Weights[2][22] = l3_2_22;
    l3Weights[2][23] = l3_2_23;
    l3Weights[2][24] = l3_2_24;
    l3Weights[2][25] = l3_2_25;
    l3Weights[2][26] = l3_2_26;
    l3Weights[2][27] = l3_2_27;
    l3Weights[2][28] = l3_2_28;
    l3Weights[2][29] = l3_2_29;
    l3Weights[2][30] = l3_2_30;
    l3Weights[2][31] = l3_2_31;
    l3Weights[2][32] = l3_2_32;
    l3Weights[2][33] = l3_2_33;
    l3Weights[2][34] = l3_2_34;
    l3Weights[2][35] = l3_2_35;
    l3Weights[2][36] = l3_2_36;
    l3Weights[2][37] = l3_2_37;
    l3Weights[2][38] = l3_2_38;
    l3Weights[2][39] = l3_2_39;
    l3Weights[2][40] = l3_2_40;
    l3Weights[2][41] = l3_2_41;
    l3Weights[2][42] = l3_2_42;
    l3Weights[2][43] = l3_2_43;
    l3Weights[2][44] = l3_2_44;
    l3Weights[2][45] = l3_2_45;
    l3Weights[2][46] = l3_2_46;
    l3Weights[2][47] = l3_2_47;
    l3Weights[2][48] = l3_2_48;
    l3Weights[2][49] = l3_2_49;
    l3Weights[2][50] = l3_2_50;
    l3Weights[2][51] = l3_2_51;
    l3Weights[2][52] = l3_2_52;
    l3Weights[2][53] = l3_2_53;
    l3Weights[2][54] = l3_2_54;
    l3Weights[2][55] = l3_2_55;
    l3Weights[2][56] = l3_2_56;
    l3Weights[2][57] = l3_2_57;
    l3Weights[2][58] = l3_2_58;
    l3Weights[2][59] = l3_2_59;
    l3Weights[2][60] = l3_2_60;
    l3Weights[2][61] = l3_2_61;
    l3Weights[2][62] = l3_2_62;
    l3Weights[2][63] = l3_2_63;
    l3Weights[3][0] = l3_3_0;
    l3Weights[3][1] = l3_3_1;
    l3Weights[3][2] = l3_3_2;
    l3Weights[3][3] = l3_3_3;
    l3Weights[3][4] = l3_3_4;
    l3Weights[3][5] = l3_3_5;
    l3Weights[3][6] = l3_3_6;
    l3Weights[3][7] = l3_3_7;
    l3Weights[3][8] = l3_3_8;
    l3Weights[3][9] = l3_3_9;
    l3Weights[3][10] = l3_3_10;
    l3Weights[3][11] = l3_3_11;
    l3Weights[3][12] = l3_3_12;
    l3Weights[3][13] = l3_3_13;
    l3Weights[3][14] = l3_3_14;
    l3Weights[3][15] = l3_3_15;
    l3Weights[3][16] = l3_3_16;
    l3Weights[3][17] = l3_3_17;
    l3Weights[3][18] = l3_3_18;
    l3Weights[3][19] = l3_3_19;
    l3Weights[3][20] = l3_3_20;
    l3Weights[3][21] = l3_3_21;
    l3Weights[3][22] = l3_3_22;
    l3Weights[3][23] = l3_3_23;
    l3Weights[3][24] = l3_3_24;
    l3Weights[3][25] = l3_3_25;
    l3Weights[3][26] = l3_3_26;
    l3Weights[3][27] = l3_3_27;
    l3Weights[3][28] = l3_3_28;
    l3Weights[3][29] = l3_3_29;
    l3Weights[3][30] = l3_3_30;
    l3Weights[3][31] = l3_3_31;
    l3Weights[3][32] = l3_3_32;
    l3Weights[3][33] = l3_3_33;
    l3Weights[3][34] = l3_3_34;
    l3Weights[3][35] = l3_3_35;
    l3Weights[3][36] = l3_3_36;
    l3Weights[3][37] = l3_3_37;
    l3Weights[3][38] = l3_3_38;
    l3Weights[3][39] = l3_3_39;
    l3Weights[3][40] = l3_3_40;
    l3Weights[3][41] = l3_3_41;
    l3Weights[3][42] = l3_3_42;
    l3Weights[3][43] = l3_3_43;
    l3Weights[3][44] = l3_3_44;
    l3Weights[3][45] = l3_3_45;
    l3Weights[3][46] = l3_3_46;
    l3Weights[3][47] = l3_3_47;
    l3Weights[3][48] = l3_3_48;
    l3Weights[3][49] = l3_3_49;
    l3Weights[3][50] = l3_3_50;
    l3Weights[3][51] = l3_3_51;
    l3Weights[3][52] = l3_3_52;
    l3Weights[3][53] = l3_3_53;
    l3Weights[3][54] = l3_3_54;
    l3Weights[3][55] = l3_3_55;
    l3Weights[3][56] = l3_3_56;
    l3Weights[3][57] = l3_3_57;
    l3Weights[3][58] = l3_3_58;
    l3Weights[3][59] = l3_3_59;
    l3Weights[3][60] = l3_3_60;
    l3Weights[3][61] = l3_3_61;
    l3Weights[3][62] = l3_3_62;
    l3Weights[3][63] = l3_3_63;
    l3Weights[4][0] = l3_4_0;
    l3Weights[4][1] = l3_4_1;
    l3Weights[4][2] = l3_4_2;
    l3Weights[4][3] = l3_4_3;
    l3Weights[4][4] = l3_4_4;
    l3Weights[4][5] = l3_4_5;
    l3Weights[4][6] = l3_4_6;
    l3Weights[4][7] = l3_4_7;
    l3Weights[4][8] = l3_4_8;
    l3Weights[4][9] = l3_4_9;
    l3Weights[4][10] = l3_4_10;
    l3Weights[4][11] = l3_4_11;
    l3Weights[4][12] = l3_4_12;
    l3Weights[4][13] = l3_4_13;
    l3Weights[4][14] = l3_4_14;
    l3Weights[4][15] = l3_4_15;
    l3Weights[4][16] = l3_4_16;
    l3Weights[4][17] = l3_4_17;
    l3Weights[4][18] = l3_4_18;
    l3Weights[4][19] = l3_4_19;
    l3Weights[4][20] = l3_4_20;
    l3Weights[4][21] = l3_4_21;
    l3Weights[4][22] = l3_4_22;
    l3Weights[4][23] = l3_4_23;
    l3Weights[4][24] = l3_4_24;
    l3Weights[4][25] = l3_4_25;
    l3Weights[4][26] = l3_4_26;
    l3Weights[4][27] = l3_4_27;
    l3Weights[4][28] = l3_4_28;
    l3Weights[4][29] = l3_4_29;
    l3Weights[4][30] = l3_4_30;
    l3Weights[4][31] = l3_4_31;
    l3Weights[4][32] = l3_4_32;
    l3Weights[4][33] = l3_4_33;
    l3Weights[4][34] = l3_4_34;
    l3Weights[4][35] = l3_4_35;
    l3Weights[4][36] = l3_4_36;
    l3Weights[4][37] = l3_4_37;
    l3Weights[4][38] = l3_4_38;
    l3Weights[4][39] = l3_4_39;
    l3Weights[4][40] = l3_4_40;
    l3Weights[4][41] = l3_4_41;
    l3Weights[4][42] = l3_4_42;
    l3Weights[4][43] = l3_4_43;
    l3Weights[4][44] = l3_4_44;
    l3Weights[4][45] = l3_4_45;
    l3Weights[4][46] = l3_4_46;
    l3Weights[4][47] = l3_4_47;
    l3Weights[4][48] = l3_4_48;
    l3Weights[4][49] = l3_4_49;
    l3Weights[4][50] = l3_4_50;
    l3Weights[4][51] = l3_4_51;
    l3Weights[4][52] = l3_4_52;
    l3Weights[4][53] = l3_4_53;
    l3Weights[4][54] = l3_4_54;
    l3Weights[4][55] = l3_4_55;
    l3Weights[4][56] = l3_4_56;
    l3Weights[4][57] = l3_4_57;
    l3Weights[4][58] = l3_4_58;
    l3Weights[4][59] = l3_4_59;
    l3Weights[4][60] = l3_4_60;
    l3Weights[4][61] = l3_4_61;
    l3Weights[4][62] = l3_4_62;
    l3Weights[4][63] = l3_4_63;
    l3Weights[5][0] = l3_5_0;
    l3Weights[5][1] = l3_5_1;
    l3Weights[5][2] = l3_5_2;
    l3Weights[5][3] = l3_5_3;
    l3Weights[5][4] = l3_5_4;
    l3Weights[5][5] = l3_5_5;
    l3Weights[5][6] = l3_5_6;
    l3Weights[5][7] = l3_5_7;
    l3Weights[5][8] = l3_5_8;
    l3Weights[5][9] = l3_5_9;
    l3Weights[5][10] = l3_5_10;
    l3Weights[5][11] = l3_5_11;
    l3Weights[5][12] = l3_5_12;
    l3Weights[5][13] = l3_5_13;
    l3Weights[5][14] = l3_5_14;
    l3Weights[5][15] = l3_5_15;
    l3Weights[5][16] = l3_5_16;
    l3Weights[5][17] = l3_5_17;
    l3Weights[5][18] = l3_5_18;
    l3Weights[5][19] = l3_5_19;
    l3Weights[5][20] = l3_5_20;
    l3Weights[5][21] = l3_5_21;
    l3Weights[5][22] = l3_5_22;
    l3Weights[5][23] = l3_5_23;
    l3Weights[5][24] = l3_5_24;
    l3Weights[5][25] = l3_5_25;
    l3Weights[5][26] = l3_5_26;
    l3Weights[5][27] = l3_5_27;
    l3Weights[5][28] = l3_5_28;
    l3Weights[5][29] = l3_5_29;
    l3Weights[5][30] = l3_5_30;
    l3Weights[5][31] = l3_5_31;
    l3Weights[5][32] = l3_5_32;
    l3Weights[5][33] = l3_5_33;
    l3Weights[5][34] = l3_5_34;
    l3Weights[5][35] = l3_5_35;
    l3Weights[5][36] = l3_5_36;
    l3Weights[5][37] = l3_5_37;
    l3Weights[5][38] = l3_5_38;
    l3Weights[5][39] = l3_5_39;
    l3Weights[5][40] = l3_5_40;
    l3Weights[5][41] = l3_5_41;
    l3Weights[5][42] = l3_5_42;
    l3Weights[5][43] = l3_5_43;
    l3Weights[5][44] = l3_5_44;
    l3Weights[5][45] = l3_5_45;
    l3Weights[5][46] = l3_5_46;
    l3Weights[5][47] = l3_5_47;
    l3Weights[5][48] = l3_5_48;
    l3Weights[5][49] = l3_5_49;
    l3Weights[5][50] = l3_5_50;
    l3Weights[5][51] = l3_5_51;
    l3Weights[5][52] = l3_5_52;
    l3Weights[5][53] = l3_5_53;
    l3Weights[5][54] = l3_5_54;
    l3Weights[5][55] = l3_5_55;
    l3Weights[5][56] = l3_5_56;
    l3Weights[5][57] = l3_5_57;
    l3Weights[5][58] = l3_5_58;
    l3Weights[5][59] = l3_5_59;
    l3Weights[5][60] = l3_5_60;
    l3Weights[5][61] = l3_5_61;
    l3Weights[5][62] = l3_5_62;
    l3Weights[5][63] = l3_5_63;
    l3Weights[6][0] = l3_6_0;
    l3Weights[6][1] = l3_6_1;
    l3Weights[6][2] = l3_6_2;
    l3Weights[6][3] = l3_6_3;
    l3Weights[6][4] = l3_6_4;
    l3Weights[6][5] = l3_6_5;
    l3Weights[6][6] = l3_6_6;
    l3Weights[6][7] = l3_6_7;
    l3Weights[6][8] = l3_6_8;
    l3Weights[6][9] = l3_6_9;
    l3Weights[6][10] = l3_6_10;
    l3Weights[6][11] = l3_6_11;
    l3Weights[6][12] = l3_6_12;
    l3Weights[6][13] = l3_6_13;
    l3Weights[6][14] = l3_6_14;
    l3Weights[6][15] = l3_6_15;
    l3Weights[6][16] = l3_6_16;
    l3Weights[6][17] = l3_6_17;
    l3Weights[6][18] = l3_6_18;
    l3Weights[6][19] = l3_6_19;
    l3Weights[6][20] = l3_6_20;
    l3Weights[6][21] = l3_6_21;
    l3Weights[6][22] = l3_6_22;
    l3Weights[6][23] = l3_6_23;
    l3Weights[6][24] = l3_6_24;
    l3Weights[6][25] = l3_6_25;
    l3Weights[6][26] = l3_6_26;
    l3Weights[6][27] = l3_6_27;
    l3Weights[6][28] = l3_6_28;
    l3Weights[6][29] = l3_6_29;
    l3Weights[6][30] = l3_6_30;
    l3Weights[6][31] = l3_6_31;
    l3Weights[6][32] = l3_6_32;
    l3Weights[6][33] = l3_6_33;
    l3Weights[6][34] = l3_6_34;
    l3Weights[6][35] = l3_6_35;
    l3Weights[6][36] = l3_6_36;
    l3Weights[6][37] = l3_6_37;
    l3Weights[6][38] = l3_6_38;
    l3Weights[6][39] = l3_6_39;
    l3Weights[6][40] = l3_6_40;
    l3Weights[6][41] = l3_6_41;
    l3Weights[6][42] = l3_6_42;
    l3Weights[6][43] = l3_6_43;
    l3Weights[6][44] = l3_6_44;
    l3Weights[6][45] = l3_6_45;
    l3Weights[6][46] = l3_6_46;
    l3Weights[6][47] = l3_6_47;
    l3Weights[6][48] = l3_6_48;
    l3Weights[6][49] = l3_6_49;
    l3Weights[6][50] = l3_6_50;
    l3Weights[6][51] = l3_6_51;
    l3Weights[6][52] = l3_6_52;
    l3Weights[6][53] = l3_6_53;
    l3Weights[6][54] = l3_6_54;
    l3Weights[6][55] = l3_6_55;
    l3Weights[6][56] = l3_6_56;
    l3Weights[6][57] = l3_6_57;
    l3Weights[6][58] = l3_6_58;
    l3Weights[6][59] = l3_6_59;
    l3Weights[6][60] = l3_6_60;
    l3Weights[6][61] = l3_6_61;
    l3Weights[6][62] = l3_6_62;
    l3Weights[6][63] = l3_6_63;
    l3Weights[7][0] = l3_7_0;
    l3Weights[7][1] = l3_7_1;
    l3Weights[7][2] = l3_7_2;
    l3Weights[7][3] = l3_7_3;
    l3Weights[7][4] = l3_7_4;
    l3Weights[7][5] = l3_7_5;
    l3Weights[7][6] = l3_7_6;
    l3Weights[7][7] = l3_7_7;
    l3Weights[7][8] = l3_7_8;
    l3Weights[7][9] = l3_7_9;
    l3Weights[7][10] = l3_7_10;
    l3Weights[7][11] = l3_7_11;
    l3Weights[7][12] = l3_7_12;
    l3Weights[7][13] = l3_7_13;
    l3Weights[7][14] = l3_7_14;
    l3Weights[7][15] = l3_7_15;
    l3Weights[7][16] = l3_7_16;
    l3Weights[7][17] = l3_7_17;
    l3Weights[7][18] = l3_7_18;
    l3Weights[7][19] = l3_7_19;
    l3Weights[7][20] = l3_7_20;
    l3Weights[7][21] = l3_7_21;
    l3Weights[7][22] = l3_7_22;
    l3Weights[7][23] = l3_7_23;
    l3Weights[7][24] = l3_7_24;
    l3Weights[7][25] = l3_7_25;
    l3Weights[7][26] = l3_7_26;
    l3Weights[7][27] = l3_7_27;
    l3Weights[7][28] = l3_7_28;
    l3Weights[7][29] = l3_7_29;
    l3Weights[7][30] = l3_7_30;
    l3Weights[7][31] = l3_7_31;
    l3Weights[7][32] = l3_7_32;
    l3Weights[7][33] = l3_7_33;
    l3Weights[7][34] = l3_7_34;
    l3Weights[7][35] = l3_7_35;
    l3Weights[7][36] = l3_7_36;
    l3Weights[7][37] = l3_7_37;
    l3Weights[7][38] = l3_7_38;
    l3Weights[7][39] = l3_7_39;
    l3Weights[7][40] = l3_7_40;
    l3Weights[7][41] = l3_7_41;
    l3Weights[7][42] = l3_7_42;
    l3Weights[7][43] = l3_7_43;
    l3Weights[7][44] = l3_7_44;
    l3Weights[7][45] = l3_7_45;
    l3Weights[7][46] = l3_7_46;
    l3Weights[7][47] = l3_7_47;
    l3Weights[7][48] = l3_7_48;
    l3Weights[7][49] = l3_7_49;
    l3Weights[7][50] = l3_7_50;
    l3Weights[7][51] = l3_7_51;
    l3Weights[7][52] = l3_7_52;
    l3Weights[7][53] = l3_7_53;
    l3Weights[7][54] = l3_7_54;
    l3Weights[7][55] = l3_7_55;
    l3Weights[7][56] = l3_7_56;
    l3Weights[7][57] = l3_7_57;
    l3Weights[7][58] = l3_7_58;
    l3Weights[7][59] = l3_7_59;
    l3Weights[7][60] = l3_7_60;
    l3Weights[7][61] = l3_7_61;
    l3Weights[7][62] = l3_7_62;
    l3Weights[7][63] = l3_7_63;

    l3Weights[0][0] = 0.4205589785047989;
    l3Weights[0][1] = -0.9724152383087714;
    l3Weights[0][2] = 0.2574134343558338;
    l3Weights[0][3] = -0.92573509322646;
    l3Weights[0][4] = -0.6107419059906947;
    l3Weights[0][5] = -0.6179684376984379;
    l3Weights[0][6] = -0.7007288164561346;
    l3Weights[0][7] = -0.9634276636899938;
    l3Weights[0][8] = -0.27470407560945137;
    l3Weights[0][9] = 1.0435220306048736;
    l3Weights[0][10] = -0.3809212444277966;
    l3Weights[0][11] = -0.049651691958255424;
    l3Weights[0][12] = 1.06499243629723;
    l3Weights[0][13] = -0.3103434739072815;
    l3Weights[0][14] = -2.1814857554142385e-07;
    l3Weights[0][15] = 0.12945591305444268;
    l3Weights[0][16] = -0.9535911418071403;
    l3Weights[0][17] = 0.45863160717161705;
    l3Weights[0][18] = -0.6164418201878894;
    l3Weights[0][19] = 0.5381857815366741;
    l3Weights[0][20] = 5.514275525718809e-09;
    l3Weights[0][21] = 0.9350075841810639;
    l3Weights[0][22] = -0.9246320411301239;
    l3Weights[0][23] = 0.9438648229714093;
    l3Weights[0][24] = -0.48280688004047284;
    l3Weights[0][25] = 0.31162066967806396;
    l3Weights[0][26] = 0.44717640881826487;
    l3Weights[0][27] = -0.7347959790233326;
    l3Weights[0][28] = -4.4462768487370617e-07;
    l3Weights[0][29] = 0.36769249258840114;
    l3Weights[0][30] = 1.0297012912973251;
    l3Weights[0][31] = -0.2653192483874822;
    l3Weights[0][32] = 0.0678996056800183;
    l3Weights[0][33] = -0.059122986173525;
    l3Weights[0][34] = -0.02214756390192362;
    l3Weights[0][35] = 0.07179667027400026;
    l3Weights[0][36] = -0.02643585141759982;
    l3Weights[0][37] = 0.008641892977904816;
    l3Weights[0][38] = -0.015789946800427387;
    l3Weights[0][39] = 0.01178027979421063;
    l3Weights[0][40] = -0.014542712880369978;
    l3Weights[0][41] = -0.03437512748773814;
    l3Weights[0][42] = 0.025010024648038046;
    l3Weights[0][43] = -0.024675839872598474;
    l3Weights[0][44] = 0.00854375736456386;
    l3Weights[0][45] = -0.06890434182102287;
    l3Weights[0][46] = -0.053721348318595916;
    l3Weights[0][47] = 0.013865084889238266;
    l3Weights[0][48] = 0.002558122582890064;
    l3Weights[0][49] = 0.06382028164503713;
    l3Weights[0][50] = -0.015065928849639632;
    l3Weights[0][51] = 0.0011383524619648579;
    l3Weights[0][52] = 0.014046220064549895;
    l3Weights[0][53] = -0.058065013161067824;
    l3Weights[0][54] = 0.033309382175429704;
    l3Weights[0][55] = -0.01657705574495109;
    l3Weights[0][56] = 0.023658110620221058;
    l3Weights[0][57] = 0.038865997718028296;
    l3Weights[0][58] = 0.013298372154238224;
    l3Weights[0][59] = 0.01596996963051731;
    l3Weights[0][60] = -0.011755719035325192;
    l3Weights[0][61] = 0.001267957777302248;
    l3Weights[0][62] = 0.03446474334266837;
    l3Weights[0][63] = -0.009465395141727277;
    l3Weights[1][0] = -0.9877093626928691;
    l3Weights[1][1] = -0.958535008098573;
    l3Weights[1][2] = -0.3382870981205218;
    l3Weights[1][3] = -0.8691203282108011;
    l3Weights[1][4] = 0.9476827723799903;
    l3Weights[1][5] = -0.9893207214512183;
    l3Weights[1][6] = -0.9216713474253293;
    l3Weights[1][7] = -0.9769349857970941;
    l3Weights[1][8] = -1.0425327322088824;
    l3Weights[1][9] = -0.9131547612459873;
    l3Weights[1][10] = -1.2437674257507345e-07;
    l3Weights[1][11] = 1.0078572268087118;
    l3Weights[1][12] = -0.7824465735105747;
    l3Weights[1][13] = -1.0580385876347053e-06;
    l3Weights[1][14] = 0.9080951068770026;
    l3Weights[1][15] = -0.6008262660128318;
    l3Weights[1][16] = -0.3717487570677512;
    l3Weights[1][17] = -0.28373896341600785;
    l3Weights[1][18] = 0.28817729718979246;
    l3Weights[1][19] = -0.5713836604166632;
    l3Weights[1][20] = 0.9092381289746915;
    l3Weights[1][21] = 0.9227251861729912;
    l3Weights[1][22] = -1.0568593872388028;
    l3Weights[1][23] = -0.8920442447008806;
    l3Weights[1][24] = -1.0163376858194735;
    l3Weights[1][25] = -1.0815900956594031;
    l3Weights[1][26] = 0.564369535076755;
    l3Weights[1][27] = -0.5771804968322984;
    l3Weights[1][28] = 0.7935012599665545;
    l3Weights[1][29] = -0.6394827831996546;
    l3Weights[1][30] = -0.24907457285257056;
    l3Weights[1][31] = -0.9370604919981993;
    l3Weights[1][32] = 0.06904576720619669;
    l3Weights[1][33] = -0.04510346823791902;
    l3Weights[1][34] = -0.1333622406715112;
    l3Weights[1][35] = -0.08937747026578403;
    l3Weights[1][36] = -0.27589168512774737;
    l3Weights[1][37] = 0.06433668456388876;
    l3Weights[1][38] = 0.4628005258963571;
    l3Weights[1][39] = 0.1853847258208429;
    l3Weights[1][40] = 0.04625350563436941;
    l3Weights[1][41] = 0.10490130345255397;
    l3Weights[1][42] = -0.16664126443146798;
    l3Weights[1][43] = 0.5365789086200582;
    l3Weights[1][44] = 0.10345809003854192;
    l3Weights[1][45] = -0.22308408142071098;
    l3Weights[1][46] = 0.43439361620516503;
    l3Weights[1][47] = 0.12430808975851206;
    l3Weights[1][48] = 0.0689338695726149;
    l3Weights[1][49] = 0.09004183755331954;
    l3Weights[1][50] = 0.1534400171703478;
    l3Weights[1][51] = -0.022816810443713646;
    l3Weights[1][52] = 0.24897552643852325;
    l3Weights[1][53] = -0.013618206430247774;
    l3Weights[1][54] = 0.10101562052512046;
    l3Weights[1][55] = -0.006653764562118377;
    l3Weights[1][56] = 0.23036130246730555;
    l3Weights[1][57] = 0.0101601113051427;
    l3Weights[1][58] = -0.01773722207278512;
    l3Weights[1][59] = 0.18997888451592657;
    l3Weights[1][60] = 0.28431093741992053;
    l3Weights[1][61] = 0.2560704586929449;
    l3Weights[1][62] = 0.17038336040120786;
    l3Weights[1][63] = -0.07223771479531346;
    l3Weights[2][0] = 0.959979102695203;
    l3Weights[2][1] = 3.4749603720091265e-07;
    l3Weights[2][2] = 1.084764898082797;
    l3Weights[2][3] = 0.4056656559629163;
    l3Weights[2][4] = 0.9607535491180774;
    l3Weights[2][5] = 1.073727387544343;
    l3Weights[2][6] = -0.9130995850126786;
    l3Weights[2][7] = 0.4524207923382843;
    l3Weights[2][8] = -1.0194987601276382;
    l3Weights[2][9] = 0.9404600943718725;
    l3Weights[2][10] = -0.250093065806124;
    l3Weights[2][11] = -0.9358200784258115;
    l3Weights[2][12] = -0.943897046574184;
    l3Weights[2][13] = -1.028592807651613;
    l3Weights[2][14] = -0.595836056634622;
    l3Weights[2][15] = -0.9751593820315236;
    l3Weights[2][16] = -0.5589467222249825;
    l3Weights[2][17] = 0.987911422495486;
    l3Weights[2][18] = -1.0287366961155782;
    l3Weights[2][19] = 0.9691807915632177;
    l3Weights[2][20] = -6.364354528004609e-07;
    l3Weights[2][21] = -0.3596635649377858;
    l3Weights[2][22] = -0.992894976029899;
    l3Weights[2][23] = -1.642961968677239e-07;
    l3Weights[2][24] = 0.9324484398858156;
    l3Weights[2][25] = 0.7245669435030118;
    l3Weights[2][26] = -0.7487923932474061;
    l3Weights[2][27] = 4.540682141552883e-07;
    l3Weights[2][28] = -0.9479467496745639;
    l3Weights[2][29] = -0.9376094949875329;
    l3Weights[2][30] = 1.0353016350017419;
    l3Weights[2][31] = -0.9936711628550609;
    l3Weights[2][32] = 0.0036503995952935618;
    l3Weights[2][33] = -0.20673020467641048;
    l3Weights[2][34] = -0.14107364016102306;
    l3Weights[2][35] = -0.28503805909986346;
    l3Weights[2][36] = 0.06495490666874607;
    l3Weights[2][37] = 0.33344782640116405;
    l3Weights[2][38] = 0.11498316172785132;
    l3Weights[2][39] = 0.6375904387957892;
    l3Weights[2][40] = 0.17082621902106995;
    l3Weights[2][41] = -0.015196191681636299;
    l3Weights[2][42] = 0.20156530351456506;
    l3Weights[2][43] = 0.24774474994117177;
    l3Weights[2][44] = 0.18229491251745422;
    l3Weights[2][45] = 0.1862100344652763;
    l3Weights[2][46] = 0.06087085225435186;
    l3Weights[2][47] = -0.20560295680156343;
    l3Weights[2][48] = 0.012764655307479202;
    l3Weights[2][49] = -0.10859786251701865;
    l3Weights[2][50] = 0.24212660318072227;
    l3Weights[2][51] = 0.06648973468084494;
    l3Weights[2][52] = -0.097885382030006;
    l3Weights[2][53] = 0.40906942842777316;
    l3Weights[2][54] = -0.07617518732659018;
    l3Weights[2][55] = -0.008400367084843341;
    l3Weights[2][56] = -0.007663709039906653;
    l3Weights[2][57] = -0.09023937988828576;
    l3Weights[2][58] = -0.21069656535304937;
    l3Weights[2][59] = 0.003620452705110935;
    l3Weights[2][60] = -0.033501676652142966;
    l3Weights[2][61] = 0.29178760062611364;
    l3Weights[2][62] = 0.012999574295282083;
    l3Weights[2][63] = -0.05130965816814442;
    l3Weights[3][0] = -0.9680462227245084;
    l3Weights[3][1] = -0.972858054014276;
    l3Weights[3][2] = -6.913236615139168e-07;
    l3Weights[3][3] = 1.0330050483856148;
    l3Weights[3][4] = -1.0323877270953405;
    l3Weights[3][5] = -0.3971322792965008;
    l3Weights[3][6] = 0.6908055443785981;
    l3Weights[3][7] = -0.7854056302502322;
    l3Weights[3][8] = -0.9656919099608964;
    l3Weights[3][9] = 0.9543258477252189;
    l3Weights[3][10] = 0.9429100598996503;
    l3Weights[3][11] = -1.036607555143086;
    l3Weights[3][12] = 1.056383138097217;
    l3Weights[3][13] = 0.9308579923349369;
    l3Weights[3][14] = 1.025802214843239;
    l3Weights[3][15] = 0.33934379209349097;
    l3Weights[3][16] = 0.5735059900315466;
    l3Weights[3][17] = -0.9843576871523237;
    l3Weights[3][18] = 0.6535995066766446;
    l3Weights[3][19] = 1.0101420925216231;
    l3Weights[3][20] = -0.31620424412151216;
    l3Weights[3][21] = -0.9642180555452242;
    l3Weights[3][22] = -1.006709710936873;
    l3Weights[3][23] = 1.0109386705593515;
    l3Weights[3][24] = 1.023818343585535;
    l3Weights[3][25] = 0.6172851585094176;
    l3Weights[3][26] = -0.8687810582921099;
    l3Weights[3][27] = 8.737865593635154e-07;
    l3Weights[3][28] = -0.7394828802706523;
    l3Weights[3][29] = -0.8254666141881022;
    l3Weights[3][30] = -6.355937953765704e-07;
    l3Weights[3][31] = 0.9195720789856067;
    l3Weights[3][32] = 0.2629415876083243;
    l3Weights[3][33] = -0.12634315921362646;
    l3Weights[3][34] = 0.04029327317183896;
    l3Weights[3][35] = 0.24681557472223337;
    l3Weights[3][36] = 0.05191934414050028;
    l3Weights[3][37] = -0.3925723825871704;
    l3Weights[3][38] = 0.02472445851798246;
    l3Weights[3][39] = 0.7427505815768187;
    l3Weights[3][40] = -0.07665745837750675;
    l3Weights[3][41] = 0.17348410017078877;
    l3Weights[3][42] = 0.3093034002801046;
    l3Weights[3][43] = -0.21947640137659863;
    l3Weights[3][44] = -0.17572194088813406;
    l3Weights[3][45] = -0.11553411131889384;
    l3Weights[3][46] = 0.3285532753870297;
    l3Weights[3][47] = 0.12891794174175988;
    l3Weights[3][48] = -0.3001726334596787;
    l3Weights[3][49] = -0.011818041842224217;
    l3Weights[3][50] = -0.13690761944527366;
    l3Weights[3][51] = 0.042687236005412466;
    l3Weights[3][52] = 0.03186211359165092;
    l3Weights[3][53] = -0.14028500100013025;
    l3Weights[3][54] = -0.3207733850262953;
    l3Weights[3][55] = 0.015189036535859146;
    l3Weights[3][56] = -0.1566336483465937;
    l3Weights[3][57] = -0.08162749640226523;
    l3Weights[3][58] = 0.12093100530578832;
    l3Weights[3][59] = -0.15048791660339092;
    l3Weights[3][60] = -0.11642400128570578;
    l3Weights[3][61] = -0.046932946227570216;
    l3Weights[3][62] = -0.0566439013387556;
    l3Weights[3][63] = 0.07277606609266762;
    l3Weights[4][0] = -0.6557104976890166;
    l3Weights[4][1] = -0.5192156495092047;
    l3Weights[4][2] = -0.9298946918288561;
    l3Weights[4][3] = -1.0746749207373612;
    l3Weights[4][4] = 0.9466258145762537;
    l3Weights[4][5] = -0.46155334236393847;
    l3Weights[4][6] = -1.140978137868218;
    l3Weights[4][7] = 0.6027929748945817;
    l3Weights[4][8] = 5.235389566001456e-07;
    l3Weights[4][9] = 0.5458997776317414;
    l3Weights[4][10] = -1.2021823543976183e-06;
    l3Weights[4][11] = 0.9104988274923463;
    l3Weights[4][12] = 6.547717547693225e-07;
    l3Weights[4][13] = -0.9848016255869626;
    l3Weights[4][14] = -0.9855275290252334;
    l3Weights[4][15] = -0.7366546955017149;
    l3Weights[4][16] = 0.8171954365977934;
    l3Weights[4][17] = 1.0706564311135947;
    l3Weights[4][18] = -1.031807047865666;
    l3Weights[4][19] = 0.7599102185574492;
    l3Weights[4][20] = -0.9495261237570738;
    l3Weights[4][21] = -1.0033691114740677;
    l3Weights[4][22] = -0.5684614918211476;
    l3Weights[4][23] = -1.072778886084396;
    l3Weights[4][24] = 0.8813920354440107;
    l3Weights[4][25] = 0.5363212752707104;
    l3Weights[4][26] = 1.04752496189233;
    l3Weights[4][27] = 0.8208641106849102;
    l3Weights[4][28] = -1.0397080392175715;
    l3Weights[4][29] = 8.973775625362554e-07;
    l3Weights[4][30] = 1.0150276953744457;
    l3Weights[4][31] = -0.1754315438355909;
    l3Weights[4][32] = 0.008924133843535274;
    l3Weights[4][33] = -0.3051478519709387;
    l3Weights[4][34] = -0.3793576646253365;
    l3Weights[4][35] = 0.029889716635677036;
    l3Weights[4][36] = 0.1344397002757851;
    l3Weights[4][37] = 0.04985530740799231;
    l3Weights[4][38] = 0.20236437165637566;
    l3Weights[4][39] = 0.5225322303638013;
    l3Weights[4][40] = 0.5982577578208398;
    l3Weights[4][41] = -0.3669720780122405;
    l3Weights[4][42] = 0.09166999280890044;
    l3Weights[4][43] = 0.3554196402412397;
    l3Weights[4][44] = 0.02685906951456436;
    l3Weights[4][45] = 0.04464127130088106;
    l3Weights[4][46] = -0.12141166226599963;
    l3Weights[4][47] = 0.1976196003275957;
    l3Weights[4][48] = -0.07691789763217889;
    l3Weights[4][49] = -0.2245919007652885;
    l3Weights[4][50] = 0.011862018910038406;
    l3Weights[4][51] = -0.0588494931844415;
    l3Weights[4][52] = 0.006487847371081175;
    l3Weights[4][53] = -0.3203879895096618;
    l3Weights[4][54] = 0.20277510521702535;
    l3Weights[4][55] = 0.005484221286412914;
    l3Weights[4][56] = 0.054112423557357694;
    l3Weights[4][57] = -0.3163492714734407;
    l3Weights[4][58] = -0.21135688526535848;
    l3Weights[4][59] = 0.30652552421286205;
    l3Weights[4][60] = 0.5693502252501975;
    l3Weights[4][61] = -0.00932588865567251;
    l3Weights[4][62] = 0.1712028037532658;
    l3Weights[4][63] = 0.1595633627609598;
    l3Weights[5][0] = 0.9491217123775697;
    l3Weights[5][1] = 0.5950174864418017;
    l3Weights[5][2] = 0.9805770669195973;
    l3Weights[5][3] = 0.6907127642188133;
    l3Weights[5][4] = -0.8229250648936941;
    l3Weights[5][5] = 0.5782655004337491;
    l3Weights[5][6] = -0.7465736324320514;
    l3Weights[5][7] = 1.0057519662399628;
    l3Weights[5][8] = -1.0079009332177495;
    l3Weights[5][9] = 0.9973228841247057;
    l3Weights[5][10] = -0.7385207158730831;
    l3Weights[5][11] = 0.6373041209212816;
    l3Weights[5][12] = -0.8241601078160049;
    l3Weights[5][13] = 0.5076601729317903;
    l3Weights[5][14] = -0.8520121722283907;
    l3Weights[5][15] = -1.0285829447845494;
    l3Weights[5][16] = -0.9185538967920973;
    l3Weights[5][17] = 0.9518233165688736;
    l3Weights[5][18] = -1.039471463648843;
    l3Weights[5][19] = 0.5790350590424788;
    l3Weights[5][20] = 0.5679849348625042;
    l3Weights[5][21] = 0.6443251582627187;
    l3Weights[5][22] = 1.0674565034765002;
    l3Weights[5][23] = -0.8433005247728929;
    l3Weights[5][24] = 0.2774862150341219;
    l3Weights[5][25] = 0.8674421566825485;
    l3Weights[5][26] = 0.42163532215811694;
    l3Weights[5][27] = -2.921441117008432e-07;
    l3Weights[5][28] = 0.9530508139927383;
    l3Weights[5][29] = 1.0048622273623908;
    l3Weights[5][30] = -0.6361043787773061;
    l3Weights[5][31] = 0.8687603273348155;
    l3Weights[5][32] = 0.14736349624518838;
    l3Weights[5][33] = -0.36883768489485874;
    l3Weights[5][34] = -0.010299784764984011;
    l3Weights[5][35] = 0.5643865302456446;
    l3Weights[5][36] = 0.344951327903253;
    l3Weights[5][37] = -0.06816933175014343;
    l3Weights[5][38] = 0.1999215751217537;
    l3Weights[5][39] = -0.24645148982987988;
    l3Weights[5][40] = 0.09301456451157522;
    l3Weights[5][41] = 0.1209710738144386;
    l3Weights[5][42] = 0.05675310552266738;
    l3Weights[5][43] = 0.20847404902667035;
    l3Weights[5][44] = 0.015444153728584138;
    l3Weights[5][45] = -0.08824430826581552;
    l3Weights[5][46] = 0.030010691636480104;
    l3Weights[5][47] = -0.8912528263562601;
    l3Weights[5][48] = -0.04752293693803715;
    l3Weights[5][49] = -0.3128836389289963;
    l3Weights[5][50] = -0.11759425909428127;
    l3Weights[5][51] = -0.04829420593913063;
    l3Weights[5][52] = -0.03512055298350629;
    l3Weights[5][53] = -0.484260034456873;
    l3Weights[5][54] = -0.15878668111213093;
    l3Weights[5][55] = -0.19255454210539544;
    l3Weights[5][56] = -0.02200906520317323;
    l3Weights[5][57] = -0.011646452817955965;
    l3Weights[5][58] = -0.0473027975024835;
    l3Weights[5][59] = -0.16160044310420524;
    l3Weights[5][60] = 0.09801537910101273;
    l3Weights[5][61] = -0.19778759301427493;
    l3Weights[5][62] = -0.10015020392561322;
    l3Weights[5][63] = 0.14523270990774886;
    l3Weights[6][0] = -0.5917755452742659;
    l3Weights[6][1] = -0.9211886827282143;
    l3Weights[6][2] = -0.5302273939622478;
    l3Weights[6][3] = -0.9288268157420634;
    l3Weights[6][4] = 1.078419011724503;
    l3Weights[6][5] = -0.44178576723145974;
    l3Weights[6][6] = -0.5940929528139485;
    l3Weights[6][7] = -0.389242364841047;
    l3Weights[6][8] = -1.348714089230758e-07;
    l3Weights[6][9] = 0.6852760490196332;
    l3Weights[6][10] = 0.6246472648345636;
    l3Weights[6][11] = 0.2305505211741427;
    l3Weights[6][12] = -0.6711710397033939;
    l3Weights[6][13] = -1.0705308960910966;
    l3Weights[6][14] = 3.399006227374832e-07;
    l3Weights[6][15] = -1.1239019914750713e-07;
    l3Weights[6][16] = -0.5166347212050546;
    l3Weights[6][17] = -0.5680118756259857;
    l3Weights[6][18] = -0.8348073462202165;
    l3Weights[6][19] = -0.9827698226694269;
    l3Weights[6][20] = 0.8313580840917886;
    l3Weights[6][21] = 0.6862241654830417;
    l3Weights[6][22] = 0.6425882231667792;
    l3Weights[6][23] = -0.6561833451167365;
    l3Weights[6][24] = 0.7297372467231102;
    l3Weights[6][25] = 0.6261168685905039;
    l3Weights[6][26] = -0.6943247043870321;
    l3Weights[6][27] = 0.6135480523380892;
    l3Weights[6][28] = -0.8266902730419384;
    l3Weights[6][29] = -0.7004219439030461;
    l3Weights[6][30] = 1.0025749992427992;
    l3Weights[6][31] = 0.7858129338216193;
    l3Weights[6][32] = 0.0022088704745482778;
    l3Weights[6][33] = 0.8169173851378473;
    l3Weights[6][34] = -0.3458535348005169;
    l3Weights[6][35] = -0.017686446774981244;
    l3Weights[6][36] = -0.2351263598536046;
    l3Weights[6][37] = 0.19959991265533947;
    l3Weights[6][38] = -0.19977077499212595;
    l3Weights[6][39] = -0.48725090697569745;
    l3Weights[6][40] = -0.5130592799127293;
    l3Weights[6][41] = -0.15445268610477283;
    l3Weights[6][42] = 0.056319453247161075;
    l3Weights[6][43] = 0.17140010730017216;
    l3Weights[6][44] = 0.10883702388277242;
    l3Weights[6][45] = 0.13978291881722377;
    l3Weights[6][46] = -0.6283663830914306;
    l3Weights[6][47] = 0.2134772656803667;
    l3Weights[6][48] = 0.06624697634727951;
    l3Weights[6][49] = -0.025158456237727837;
    l3Weights[6][50] = -0.10759416058878185;
    l3Weights[6][51] = -0.17982949445890994;
    l3Weights[6][52] = -0.005702306731197101;
    l3Weights[6][53] = 0.24734044112113032;
    l3Weights[6][54] = -0.009451523540366422;
    l3Weights[6][55] = 0.11743892982872194;
    l3Weights[6][56] = 0.29910864741750054;
    l3Weights[6][57] = -0.11525711720299428;
    l3Weights[6][58] = 0.1796534556091059;
    l3Weights[6][59] = 0.04315764932444326;
    l3Weights[6][60] = -0.044680621088434784;
    l3Weights[6][61] = -0.04291324650247583;
    l3Weights[6][62] = 0.11043498401878986;
    l3Weights[6][63] = 0.10949628983867442;
    l3Weights[7][0] = -0.4439769460216292;
    l3Weights[7][1] = 1.0066456965440997;
    l3Weights[7][2] = -0.46454106735102646;
    l3Weights[7][3] = 1.0701902597491604;
    l3Weights[7][4] = -0.755125331064407;
    l3Weights[7][5] = 0.13559555301176132;
    l3Weights[7][6] = 0.8960383409494449;
    l3Weights[7][7] = -4.6644772005975043e-07;
    l3Weights[7][8] = -0.6063191804921356;
    l3Weights[7][9] = -0.2968886016197721;
    l3Weights[7][10] = 0.673603130784033;
    l3Weights[7][11] = 0.10377481258043177;
    l3Weights[7][12] = -0.8920875099450821;
    l3Weights[7][13] = -0.4262641770171147;
    l3Weights[7][14] = -4.105135598881807e-07;
    l3Weights[7][15] = 0.5975807311545027;
    l3Weights[7][16] = 0.949744707969621;
    l3Weights[7][17] = -8.131844199394681e-07;
    l3Weights[7][18] = -0.9935019171133052;
    l3Weights[7][19] = 0.760193561325478;
    l3Weights[7][20] = -0.5214765477265136;
    l3Weights[7][21] = -0.6626867067711356;
    l3Weights[7][22] = -7.302828268233902e-07;
    l3Weights[7][23] = 3.326734591542257e-08;
    l3Weights[7][24] = -0.7101397684652322;
    l3Weights[7][25] = -4.0974338997359245e-07;
    l3Weights[7][26] = -0.4697804149363546;
    l3Weights[7][27] = -9.97025498918533e-07;
    l3Weights[7][28] = -0.90973970582799;
    l3Weights[7][29] = -0.8467074934484702;
    l3Weights[7][30] = -0.2687181908270275;
    l3Weights[7][31] = -0.30889880900942485;
    l3Weights[7][32] = 0.06366304211251247;
    l3Weights[7][33] = 0.022793068037391385;
    l3Weights[7][34] = -0.16660170594850712;
    l3Weights[7][35] = 0.6333410161078161;
    l3Weights[7][36] = -0.1819676503204034;
    l3Weights[7][37] = -0.40226647081967837;
    l3Weights[7][38] = -0.1669879697703179;
    l3Weights[7][39] = 0.01753789002676273;
    l3Weights[7][40] = 0.20517758583465973;
    l3Weights[7][41] = 0.13743937839896334;
    l3Weights[7][42] = 0.022819688124000235;
    l3Weights[7][43] = -0.39860777814687537;
    l3Weights[7][44] = -0.03384805502503533;
    l3Weights[7][45] = -0.031541001983385136;
    l3Weights[7][46] = -0.007841592146195234;
    l3Weights[7][47] = 0.12162881994749024;
    l3Weights[7][48] = 0.3765941113941484;
    l3Weights[7][49] = 0.12713373594272012;
    l3Weights[7][50] = -0.06962627837265893;
    l3Weights[7][51] = -0.25652167615868227;
    l3Weights[7][52] = -0.1439500758986765;
    l3Weights[7][53] = 0.3398784638645812;
    l3Weights[7][54] = 0.015641694667715603;
    l3Weights[7][55] = -0.2955081328636618;
    l3Weights[7][56] = 0.13792763388182586;
    l3Weights[7][57] = 0.1356218814539645;
    l3Weights[7][58] = 0.5371809500751029;
    l3Weights[7][59] = 0.23767045529627528;
    l3Weights[7][60] = 0.023254329908700185;
    l3Weights[7][61] = 0.09124158370096867;
    l3Weights[7][62] = -0.42958676477210317;
    l3Weights[7][63] = -0.06247023705715832;


    if (!networkData) {
        assert(globalNetworkData);
        networkData = globalNetworkData;
    }

    // Reset accumulator
    resetAccumulator<Color::WHITE>(board, &accumulatorStack[0]);
    resetAccumulator<Color::BLACK>(board, &accumulatorStack[0]);
    accumulatorStack[0].numDirtyThreats = 0;

    currentAccumulator = 0;
    lastCalculatedAccumulator[Color::WHITE] = 0;
    lastCalculatedAccumulator[Color::BLACK] = 0;

    // Also reset finny tables
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < KING_BUCKETS; j++) {
            memset(finnyTable[i][j].byColor, 0, sizeof(finnyTable[i][j].byColor));
            memset(finnyTable[i][j].byPiece, 0, sizeof(finnyTable[i][j].byPiece));
            memset(finnyTable[i][j].pieceState[Color::WHITE], 0, sizeof(networkData->inputBiases));
            memset(finnyTable[i][j].pieceState[Color::BLACK], 0, sizeof(networkData->inputBiases));
        }
    }
}

template<Color side>
void NNUE::resetAccumulator(Board* board, Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));
    // Overwrite with zeroes
    memset(acc->pieceState[side], 0, sizeof(acc->pieceState[side]));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures<side>(board, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<true, side>(acc->threatState, acc->threatState, featureIndex);

    ThreatInputs::FeatureList pieceFeatures;
    ThreatInputs::addPieceFeatures(board, side, pieceFeatures, KING_BUCKET_LAYOUT);
    for (int featureIndex : pieceFeatures)
        addToAccumulator<false, side>(acc->pieceState, acc->pieceState, featureIndex);

    acc->kingBucketInfo[side] = getKingBucket(side, lsb(board->byColor[side] & board->byPiece[Piece::KING]));
    acc->board = board;
}

void NNUE::updateThreat(Piece piece, Piece attackedPiece, Square square, Square attackedSquare, Color pieceColor, Color attackedColor, bool add) {
    assert(piece != Piece::NONE);
    assert(attackedPiece != Piece::NONE);

    Accumulator* acc = &accumulatorStack[currentAccumulator];
    acc->dirtyThreats[acc->numDirtyThreats++] = DirtyThreat(piece, pieceColor, attackedPiece, attackedColor, square, attackedSquare, add);
}

void NNUE::incrementAccumulator() {
    currentAccumulator++;
    accumulatorStack[currentAccumulator].numDirtyThreats = 0;
}

void NNUE::decrementAccumulator() {
    assert(currentAccumulator > 0);
    currentAccumulator--;
    lastCalculatedAccumulator[Color::WHITE] = std::min(lastCalculatedAccumulator[Color::WHITE], currentAccumulator);
    lastCalculatedAccumulator[Color::BLACK] = std::min(lastCalculatedAccumulator[Color::BLACK], currentAccumulator);
}

void NNUE::finalizeMove(Board* board, DirtyPiece dirtyPiece) {
    Accumulator* accumulator = &accumulatorStack[currentAccumulator];
    accumulator->board = board;
    accumulator->dirtyPiece = dirtyPiece;

    for (Color side = Color::WHITE; side <= Color::BLACK; ++side) {
        accumulator->kingBucketInfo[side] = getKingBucket(side, lsb(board->byPiece[Piece::KING] & board->byColor[side]));
    }
}

template<Color side>
void NNUE::calculateAccumulators() {
    // Incrementally update all accumulators for this side
    while (lastCalculatedAccumulator[side] < currentAccumulator) {

        Accumulator* inputAcc = &accumulatorStack[lastCalculatedAccumulator[side]];
        Accumulator* outputAcc = &accumulatorStack[lastCalculatedAccumulator[side] + 1];

        KingBucketInfo* inputKingBucket = &inputAcc->kingBucketInfo[side];
        KingBucketInfo* outputKingBucket = &outputAcc->kingBucketInfo[side];

        if (inputKingBucket->mirrored != outputKingBucket->mirrored)
            refreshThreatFeatures<side>(outputAcc);
        else
            incrementallyUpdateThreatFeatures<side>(inputAcc, outputAcc, outputKingBucket);

        if (inputKingBucket->bucket != outputKingBucket->bucket || inputKingBucket->mirrored != outputKingBucket->mirrored)
            refreshPieceFeatures<side>(outputAcc, outputKingBucket);
        else
            incrementallyUpdatePieceFeatures<side>(inputAcc, outputAcc, outputKingBucket);

        lastCalculatedAccumulator[side]++;
    }
}

template<Color side>
void NNUE::refreshPieceFeatures(Accumulator* acc, KingBucketInfo* kingBucket) {
    FinnyEntry* finnyEntry = &finnyTable[kingBucket->mirrored][kingBucket->bucket];

    // Update matching finny table with the changed pieces
    for (Color c = Color::WHITE; c <= Color::BLACK; ++c) {
        for (Piece p = Piece::PAWN; p < Piece::TOTAL; ++p) {
            Bitboard finnyBB = finnyEntry->byColor[side][c] & finnyEntry->byPiece[side][p];
            Bitboard accBB = acc->board->byColor[c] & acc->board->byPiece[p];

            Bitboard addBB = accBB & ~finnyBB;
            Bitboard removeBB = ~accBB & finnyBB;

            while (addBB) {
                Square square = popLSB(&addBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (side * 56) ^ (kingBucket->mirrored * 7), static_cast<Color>(c != side), kingBucket->bucket);
                addToAccumulator<false, side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
            while (removeBB) {
                Square square = popLSB(&removeBB);
                int featureIndex = ThreatInputs::getPieceFeature(p, square ^ (side * 56) ^ (kingBucket->mirrored * 7), static_cast<Color>(c != side), kingBucket->bucket);
                subFromAccumulator<false, side>(finnyEntry->pieceState, finnyEntry->pieceState, featureIndex);
            }
        }
    }
    memcpy(finnyEntry->byColor[side], acc->board->byColor, sizeof(finnyEntry->byColor[side]));
    memcpy(finnyEntry->byPiece[side], acc->board->byPiece, sizeof(finnyEntry->byPiece[side]));

    // Copy result to the current accumulator
    memcpy(acc->pieceState[side], finnyEntry->pieceState[side], sizeof(acc->pieceState[side]));
}

template<Color side>
void NNUE::refreshThreatFeatures(Accumulator* acc) {
    // Overwrite with biases
    memcpy(acc->threatState[side], networkData->inputBiases, sizeof(networkData->inputBiases));

    ThreatInputs::FeatureList threatFeatures;
    ThreatInputs::addThreatFeatures<side>(acc->board, threatFeatures);
    for (int featureIndex : threatFeatures)
        addToAccumulator<true, side>(acc->threatState, acc->threatState, featureIndex);
}

template<Color side>
void NNUE::incrementallyUpdatePieceFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {

    Square squareFlip = (56 * side) ^ (7 * kingBucket->mirrored);
    DirtyPiece& dirtyPiece = outputAcc->dirtyPiece;
    Color color = static_cast<Color>(dirtyPiece.pieceColor != side);

    // Promotion
    if (dirtyPiece.target == NO_SQUARE) {
        int sub1 = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(dirtyPiece.addPiece, dirtyPiece.addSquare ^ squareFlip, color, kingBucket->bucket);
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);

        if (dirtyPiece.removeSquare != NO_SQUARE) {
            // Promotion capture
            int sub2 = ThreatInputs::getPieceFeature(dirtyPiece.removePiece, dirtyPiece.removeSquare ^ squareFlip, flip(color), kingBucket->bucket);
            subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
        }
    }
    // Castling
    else if (dirtyPiece.addSquare != NO_SQUARE) {
        int sub1 = ThreatInputs::getPieceFeature(Piece::KING, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(Piece::KING, dirtyPiece.target ^ squareFlip, color, kingBucket->bucket);
        int sub2 = ThreatInputs::getPieceFeature(Piece::ROOK, dirtyPiece.removeSquare ^ squareFlip, color, kingBucket->bucket);
        int add2 = ThreatInputs::getPieceFeature(Piece::ROOK, dirtyPiece.addSquare ^ squareFlip, color, kingBucket->bucket);
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);
        addSubToAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, add2, sub2);
    }
    // Other
    else {
        int sub1 = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.origin ^ squareFlip, color, kingBucket->bucket);
        int add1 = ThreatInputs::getPieceFeature(dirtyPiece.piece, dirtyPiece.target ^ squareFlip, color, kingBucket->bucket);
        addSubToAccumulator<false, side>(inputAcc->pieceState, outputAcc->pieceState, add1, sub1);

        if (dirtyPiece.removeSquare != NO_SQUARE) {
            // Capture / EP
            int sub2 = ThreatInputs::getPieceFeature(dirtyPiece.removePiece, dirtyPiece.removeSquare ^ squareFlip, flip(color), kingBucket->bucket);
            subFromAccumulator<false, side>(outputAcc->pieceState, outputAcc->pieceState, sub2);
        }
    }
}

template<Color side>
void NNUE::incrementallyUpdateThreatFeatures(Accumulator* inputAcc, Accumulator* outputAcc, KingBucketInfo* kingBucket) {
    ThreatInputs::FeatureList addFeatures, subFeatures;

    for (int dp = 0; dp < outputAcc->numDirtyThreats; dp++) {
        DirtyThreat& dt = outputAcc->dirtyThreats[dp];
        bool add = dt.attackedSquare >> 7;

        int featureIndex = ThreatInputs::getThreatFeature<side>(dt.piece, dt.attackedPiece, dt.square, dt.attackedSquare & 0b111111, kingBucket->mirrored);
        if (featureIndex < ThreatInputs::FEATURE_COUNT) {
            __builtin_prefetch(&networkData->inputThreatWeights[featureIndex * L1_SIZE]);
            (add ? addFeatures : subFeatures).add(featureIndex);
        }
    }

    if (!outputAcc->numDirtyThreats || (!addFeatures.size() && !subFeatures.size())) {
        memcpy(outputAcc->threatState[side], inputAcc->threatState[side], sizeof(networkData->inputBiases));
        return;
    }

#if defined(__AVX512F__) && defined(__AVX512BW__)

    VecI16* inputVec = (VecI16*)inputAcc->threatState[side];
    VecI16* outputVec = (VecI16*)outputAcc->threatState[side];
    VecI16 registers[L1_ITERATIONS];

    for (int i = 0; i < L1_ITERATIONS; i++)
        registers[i] = inputVec[i];

    while (addFeatures.size() && subFeatures.size()) {
        VecI16s* addWeightsVec = (VecI16s*)&networkData->inputThreatWeights[addFeatures.remove(0) * L1_SIZE];
        VecI16s* subWeightsVec = (VecI16s*)&networkData->inputThreatWeights[subFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(addWeightsVec[i]);
            VecI16 subWeights = convertEpi8Epi16(subWeightsVec[i]);
            registers[i] = subEpi16(addEpi16(registers[i], addWeights), subWeights);
        }
    }

    while (addFeatures.size()) {
        VecI16s* addWeightVec = (VecI16s*)&networkData->inputThreatWeights[addFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; i++) {
            VecI16 addWeights = convertEpi8Epi16(addWeightVec[i]);
            registers[i] = addEpi16(registers[i], addWeights);
        }
    }

    while (subFeatures.size()) {
        VecI16s* subWeightVec = (VecI16s*)&networkData->inputThreatWeights[subFeatures.remove(0) * L1_SIZE];
        for (int i = 0; i < L1_ITERATIONS; i++) {
            VecI16 subWeights = convertEpi8Epi16(subWeightVec[i]);
            registers[i] = subEpi16(registers[i], subWeights);
        }
    }

    for (int i = 0; i < L1_ITERATIONS; i++)
        outputVec[i] = registers[i];

#else

    while (addFeatures.size() && subFeatures.size()) {
        addSubToAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0), subFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (addFeatures.size()) {
        addToAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, addFeatures.remove(0));
        inputAcc = outputAcc;
    }

    while (subFeatures.size()) {
        subFromAccumulator<true, side>(inputAcc->threatState, outputAcc->threatState, subFeatures.remove(0));
        inputAcc = outputAcc;
    }

#endif
}

template<bool I8, Color side>
void NNUE::addToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* weightsVec = (VecI16s*)&networkData->inputThreatWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(weightsVec[i]);
            outputVec[i] = addEpi16(inputVec[i], addWeights);
        }
    } else {
        VecI16* weightsVec = (VecI16*)&networkData->inputPsqWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = addEpi16(inputVec[i], weightsVec[i]);
        }
    }
}

template<bool I8, Color side>
void NNUE::subFromAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int featureIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* weightsVec = (VecI16s*)&networkData->inputThreatWeights[featureIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(weightsVec[i]);
            outputVec[i] = subEpi16(inputVec[i], addWeights);
        }
    } else {
        VecI16* weightsVec = (VecI16*)&networkData->inputPsqWeights[featureIndex * L1_SIZE];
        
        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = subEpi16(inputVec[i], weightsVec[i]);
        }
    }
}

template<bool I8, Color side>
void NNUE::addSubToAccumulator(int16_t(*inputData)[L1_SIZE], int16_t(*outputData)[L1_SIZE], int addIndex, int subIndex) {
    VecI16* inputVec = (VecI16*)inputData[side];
    VecI16* outputVec = (VecI16*)outputData[side];

    if (I8) {
        VecI16s* addWeightsVec = (VecI16s*)&networkData->inputThreatWeights[addIndex * L1_SIZE];
        VecI16s* subWeightsVec = (VecI16s*)&networkData->inputThreatWeights[subIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            VecI16 addWeights = convertEpi8Epi16(addWeightsVec[i]);
            VecI16 subWeights = convertEpi8Epi16(subWeightsVec[i]);
            outputVec[i] = subEpi16(addEpi16(inputVec[i], addWeights), subWeights);
        }
    } else {
        VecI16* addWeightsVec = (VecI16*)&networkData->inputPsqWeights[addIndex * L1_SIZE];
        VecI16* subWeightsVec = (VecI16*)&networkData->inputPsqWeights[subIndex * L1_SIZE];

        for (int i = 0; i < L1_ITERATIONS; ++i) {
            outputVec[i] = subEpi16(addEpi16(inputVec[i], addWeightsVec[i]), subWeightsVec[i]);
        }
    }
}

Eval NNUE::evaluate(Board* board) {
    // Make sure the current accumulators are up to date
    calculateAccumulators<Color::WHITE>();
    calculateAccumulators<Color::BLACK>();

    assert(lastCalculatedAccumulator[Color::WHITE] == currentAccumulator && lastCalculatedAccumulator[Color::BLACK] == currentAccumulator);

    // Calculate output bucket based on piece count
    int pieceCount = BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]);
    constexpr int divisor = ((32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS);
    int bucket = (pieceCount - 2) / divisor;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Accumulator* accumulator = &accumulatorStack[currentAccumulator];

    VecI16* stmThreatAcc = reinterpret_cast<VecI16*>(accumulator->threatState[board->stm]);
    VecI16* stmPieceAcc = reinterpret_cast<VecI16*>(accumulator->pieceState[board->stm]);
    VecI16* oppThreatAcc = reinterpret_cast<VecI16*>(accumulator->threatState[1 - board->stm]);
    VecI16* oppPieceAcc = reinterpret_cast<VecI16*>(accumulator->pieceState[1 - board->stm]);

    VecI16 i16Zero = set1Epi16(0);
    VecI16 i16Quant = set1Epi16(INPUT_QUANT);

    // ---------------------- FT ACTIVATION & PAIRWISE ----------------------

    alignas(ALIGNMENT) uint8_t pairwiseOutputs[L1_SIZE];
    VecIu8* pairwiseOutputsVec = reinterpret_cast<VecIu8*>(pairwiseOutputs);

    constexpr int inverseShift = 16 - INPUT_SHIFT;
    constexpr int pairwiseOffset = L1_SIZE / I16_VEC_SIZE / 2;
    for (int pw = 0; pw < pairwiseOffset; pw += 2) {
        // STM
        VecI16 clipped1 = minEpi16(maxEpi16(addEpi16(stmPieceAcc[pw], stmThreatAcc[pw]), i16Zero), i16Quant);
        VecI16 clipped2 = minEpi16(addEpi16(stmPieceAcc[pw + pairwiseOffset], stmThreatAcc[pw + pairwiseOffset]), i16Quant);
        VecI16 shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(stmPieceAcc[pw + 1], stmThreatAcc[pw + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(stmPieceAcc[pw + 1 + pairwiseOffset], stmThreatAcc[pw + 1 + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        VecI16 mul2 = mulhiEpi16(shift, clipped2);

        pairwiseOutputsVec[pw / 2] = packusEpi16(mul1, mul2);

        // NSTM
        clipped1 = minEpi16(maxEpi16(addEpi16(oppPieceAcc[pw], oppThreatAcc[pw]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppPieceAcc[pw + pairwiseOffset], oppThreatAcc[pw + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul1 = mulhiEpi16(shift, clipped2);

        clipped1 = minEpi16(maxEpi16(addEpi16(oppPieceAcc[pw + 1], oppThreatAcc[pw + 1]), i16Zero), i16Quant);
        clipped2 = minEpi16(addEpi16(oppPieceAcc[pw + 1 + pairwiseOffset], oppThreatAcc[pw + 1 + pairwiseOffset]), i16Quant);
        shift = slliEpi16(clipped1, inverseShift);
        mul2 = mulhiEpi16(shift, clipped2);

        pairwiseOutputsVec[pw / 2 + pairwiseOffset / 2] = packusEpi16(mul1, mul2);
    }

#if defined(PROCESS_NET)
    nnz.addActivations(pairwiseOutputs);
#endif

    alignas(ALIGNMENT) int l1MatmulOutputs[L2_SIZE] = {};

    // ---------------------- NNZ COMPUTATION ----------------------

#if defined(__SSSE3__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    int nnzCount = 0;
    alignas(ALIGNMENT) uint16_t nnzIndices[L1_SIZE / INT8_PER_INT32];

    VecI32* pairwiseOutputsVecI32 = reinterpret_cast<VecI32*>(pairwiseOutputs);
    VecI16_v128 nnzZero = setZero_v128();
    VecI16_v128 nnzIncrement = set1Epi16_v128(8);
    for (int i = 0; i < L1_SIZE / INT8_PER_INT32 / 16; i++) {
        uint32_t nnzMask = 0;

        for (int j = 0; j < 16 / I32_VEC_SIZE; j++) {
            nnzMask |= vecNNZ(pairwiseOutputsVecI32[i * 16 / I32_VEC_SIZE + j]) << (j * I32_VEC_SIZE);
        }

        for (int j = 0; j < 16 / 8; j++) {
            uint16_t lookup = (nnzMask >> (j * 8)) & 0xFF;
            VecI16_v128 offsets = loadu_v128(nnzLookup[lookup]);
            storeu_v128(nnzIndices + nnzCount, addEpi16_v128(nnzZero, offsets));
            nnzCount += BB::popcount(lookup);
            nnzZero = addEpi16_v128(nnzZero, nnzIncrement);
        }
    }

    // ---------------------- SPARSE L1 PROPAGATION ----------------------

    int* pairwiseOutputsPacks = reinterpret_cast<int*>(pairwiseOutputs);
    VecI32* l1MatmulOutputsVec = reinterpret_cast<VecI32*>(l1MatmulOutputs);
    int8_t* l1Weights = networkData->l1Weights[bucket];

#if defined(__AVX512VNNI__)
    VecI32 acc0{}, acc1{};

    int i = 0;
    for (; i < nnzCount - 3; i += 4) {
        int pw_1 = nnzIndices[i];
        int pw_2 = nnzIndices[i + 1];
        int pw_3 = nnzIndices[i + 2];
        int pw_4 = nnzIndices[i + 3];
        VecIu8 u8_1 = set1Epi32(pairwiseOutputsPacks[pw_1]);
        VecIu8 u8_2 = set1Epi32(pairwiseOutputsPacks[pw_2]);
        VecIu8 u8_3 = set1Epi32(pairwiseOutputsPacks[pw_3]);
        VecIu8 u8_4 = set1Epi32(pairwiseOutputsPacks[pw_4]);
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&l1Weights[pw_1 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&l1Weights[pw_2 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_3 = reinterpret_cast<VecI8*>(&l1Weights[pw_3 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_4 = reinterpret_cast<VecI8*>(&l1Weights[pw_4 * INT8_PER_INT32 * L2_SIZE]);

        acc0 = dpbusdEpi32x2(acc0, u8_1, weights_1[0], u8_2, weights_2[0]);
        acc1 = dpbusdEpi32x2(acc1, u8_3, weights_3[0], u8_4, weights_4[0]);
    }

    l1MatmulOutputsVec[0] = _mm512_add_epi32(acc0, acc1);
#else
    int i = 0;
    for (; i < nnzCount - 1; i += 2) {
        int pw_1 = nnzIndices[i];
        int pw_2 = nnzIndices[i + 1];
        VecIu8 u8_1 = set1Epi32(pairwiseOutputsPacks[pw_1]);
        VecIu8 u8_2 = set1Epi32(pairwiseOutputsPacks[pw_2]);
        VecI8* weights_1 = reinterpret_cast<VecI8*>(&l1Weights[pw_1 * INT8_PER_INT32 * L2_SIZE]);
        VecI8* weights_2 = reinterpret_cast<VecI8*>(&l1Weights[pw_2 * INT8_PER_INT32 * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32x2(l1MatmulOutputsVec[l1], u8_1, weights_1[l1], u8_2, weights_2[l1]);
        }
    }
#endif

    for (; i < nnzCount; i++) {
        int pw = nnzIndices[i];
        VecIu8 u8 = set1Epi32(pairwiseOutputsPacks[pw]);
        VecI8* weights = reinterpret_cast<VecI8*>(&l1Weights[pw * INT8_PER_INT32 * L2_SIZE]);

        for (int l1 = 0; l1 < L2_SIZE / I32_VEC_SIZE; l1++) {
            l1MatmulOutputsVec[l1] = dpbusdEpi32(l1MatmulOutputsVec[l1], u8, weights[l1]);
        }
    }

#else
    for (int ft = 0; ft < L1_SIZE; ft++) {
        if (!pairwiseOutputs[ft])
            continue;

        for (int l1 = 0; l1 < L2_SIZE; l1++) {
            l1MatmulOutputs[l1] += pairwiseOutputs[ft] * networkData->l1Weights[bucket][ft * L2_SIZE + l1];
        }
    }
#endif

    // ---------------------- CONVERT TO FLOATS & ACTIVATE L1 ----------------------

    alignas(ALIGNMENT) float l1Outputs[2 * L2_SIZE];
#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)

    VecF psNorm = set1Ps(L1_NORMALISATION);
    VecF psZero = set1Ps(0.0f);
    VecF psOne = set1Ps(1.0f);

    VecF* l1Biases = reinterpret_cast<VecF*>(networkData->l1Biases[bucket]);
    VecF* l1OutputsVec = reinterpret_cast<VecF*>(l1Outputs);

    for (int l2 = 0; l2 < L2_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF converted = cvtepi32Ps(l1MatmulOutputsVec[l2]);
        VecF l1Result = fmaddPs(converted, psNorm, l1Biases[l2]);
        l1OutputsVec[l2] = maxPs(minPs(l1Result, psOne), psZero);
        l1OutputsVec[l2 + L2_SIZE / FLOAT_VEC_SIZE] = minPs(mulPs(l1Result, l1Result), psOne);
    }
#else
    for (int l1 = 0; l1 < L2_SIZE; l1++) {
        float l1Result = std::fma(static_cast<float>(l1MatmulOutputs[l1]), L1_NORMALISATION, networkData->l1Biases[bucket][l1]);
        l1Outputs[l1] = std::clamp(l1Result, 0.0f, 1.0f);
        l1Outputs[l1 + L2_SIZE] = std::clamp(l1Result * l1Result, 0.0f, 1.0f);
    }
#endif

    // ---------------------- L2 PROPAGATION & ACTIVATION ----------------------

    alignas(ALIGNMENT) float l2Outputs[L3_SIZE];
    memcpy(l2Outputs, networkData->l2Biases[bucket], sizeof(l2Outputs));

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    VecF* l2OutputsVec = reinterpret_cast<VecF*>(l2Outputs);
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1++) {
        VecF l1Vec = set1Ps(l1Outputs[l1]);
        VecF* weights = reinterpret_cast<VecF*>(&networkData->l2Weights[bucket][l1 * L3_SIZE]);
        for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2++) {
            l2OutputsVec[l2] = fmaddPs(l1Vec, weights[l2], l2OutputsVec[l2]);
        }
    }
    for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2++) {
        VecF l2Activated = maxPs(minPs(l2OutputsVec[l2], psOne), psZero);
        l2OutputsVec[l2] = mulPs(l2Activated, l2Activated);
    }
#else
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1++) {
        for (int l2 = 0; l2 < L3_SIZE; l2++) {
            l2Outputs[l2] = std::fma(l1Outputs[l1], networkData->l2Weights[bucket][l1 * L3_SIZE + l2], l2Outputs[l2]);
        }
    }
    for (int l2 = 0; l2 < L3_SIZE; l2++) {
        float l2Activated = std::clamp(l2Outputs[l2], 0.0f, 1.0f);
        l2Outputs[l2] = l2Activated * l2Activated;
    }
#endif

    // ---------------------- L3 PROPAGATION ----------------------

#if defined(__FMA__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(ARCH_ARM)
    constexpr int chunks = 64 / sizeof(VecF);

    VecF resultSums[chunks];
    for (int j = 0; j < chunks; j++)
        resultSums[j] = psZero;

    VecF* l3WeightsVec = reinterpret_cast<VecF*>(l3Weights[bucket]);
    for (int l2 = 0; l2 < L3_SIZE / FLOAT_VEC_SIZE; l2 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = fmaddPs(l2OutputsVec[l2 + chunk], l3WeightsVec[l2 + chunk], resultSums[chunk]);
        }
    }
    for (int l1 = 0; l1 < 2 * L2_SIZE / FLOAT_VEC_SIZE; l1 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = fmaddPs(l1OutputsVec[l1 + chunk], l3WeightsVec[L3_SIZE / FLOAT_VEC_SIZE + l1 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPs(resultSums);
#else
    constexpr int chunks = 64 / sizeof(float);
    float resultSums[chunks] = {};

    for (int l2 = 0; l2 < L3_SIZE; l2 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = std::fma(l2Outputs[l2 + chunk], l3Weights[bucket][l2 + chunk], resultSums[chunk]);
        }
    }
    for (int l1 = 0; l1 < 2 * L2_SIZE; l1 += chunks) {
        for (int chunk = 0; chunk < chunks; chunk++) {
            resultSums[chunk] = std::fma(l1Outputs[l1 + chunk], l3Weights[bucket][L3_SIZE + l1 + chunk], resultSums[chunk]);
        }
    }

    float result = networkData->l3Biases[bucket] + reduceAddPsR(resultSums, chunks);
#endif

    return result * NETWORK_SCALE;
}
