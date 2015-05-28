// Copyright (c) 2015 Pierre Moulon.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_SFM_DATA_BA_CERES_CAMERA_FUNCTOR_HPP
#define OPENMVG_SFM_DATA_BA_CERES_CAMERA_FUNCTOR_HPP

#include "openMVG/cameras/cameras.hpp"
#include "ceres/rotation.h"

//--
//- Define ceres Cost_functor for each OpenMVG camera model
//--

namespace openMVG {
namespace sfm {

/**
 * @brief Ceres functor to use a Pinhole_Intrinsic
 *
 *  Data parameter blocks are the following <2,3,6,3>
 *  - 2 => dimension of the residuals,
 *  - 3 => the intrinsic data block K[R|t] defined as [focal, principal point x, principal point y],
 *  - 6 => the camera extrinsic data block (camera orientation and position) [R;t],
 *         - rotation(angle axis), and translation [rX,rY,rZ,tx,ty,tz].
 *  - 3 => a 3D point data block.
 *
 */
struct ResidualErrorFunctor_Pinhole_Intrinsic
{
  ResidualErrorFunctor_Pinhole_Intrinsic(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }

  // Enum to map intrinsics parameters between openMVG & ceres camera data parameter block.
  enum {
    OFFSET_FOCAL_LENGTH = 0,
    OFFSET_PRINCIPAL_POINT_X = 1,
    OFFSET_PRINCIPAL_POINT_Y = 2
  };

  /**
   * @param[in] cam_K: Camera intrinsics( focal, principal point [x,y] )
   * @param[in] cam_Rt: Camera parameterized using one block of 6 parameters [R;t]:
   *   - 3 for rotation(angle axis), 3 for translation
   * @param[in] pos_3dpoint
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_K,
    const T* const cam_Rt,
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    //--
    // Apply external parameters (Pose)
    //--

    const T * cam_R = cam_Rt;
    const T * cam_t = &cam_Rt[3];

    T pos_proj[3];
    // Rotate the point according the camera rotation
    ceres::AngleAxisRotatePoint(cam_R, pos_3dpoint, pos_proj);

    // Apply the camera translation
    pos_proj[0] += cam_t[0];
    pos_proj[1] += cam_t[1];
    pos_proj[2] += cam_t[2];

    // Transform the point from homogeneous to euclidean (undistorted point)
    const T x_u = pos_proj[0] / pos_proj[2];
    const T y_u = pos_proj[1] / pos_proj[2];

    //--
    // Apply intrinsic parameters
    //--

    const T& focal = cam_K[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = cam_K[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = cam_K[OFFSET_PRINCIPAL_POINT_Y];

    // Apply focal length and principal point to get the final image coordinates
    const T projected_x = principal_point_x + focal * x_u;
    const T projected_y = principal_point_y + focal * y_u;

    // Compute and return the error is the difference between the predicted
    //  and observed position
    out_residuals[0] = projected_x - T(m_pos_2dpoint[0]);
    out_residuals[1] = projected_y - T(m_pos_2dpoint[1]);

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

/**
 * @brief Ceres functor to use a Pinhole_Intrinsic_Radial_K1
 *
 *  Data parameter blocks are the following <2,4,6,3>
 *  - 2 => dimension of the residuals,
 *  - 4 => the intrinsic data block [focal, principal point x, principal point y, K1],
 *  - 6 => the camera extrinsic data block (camera orientation and position) [R;t],
 *         - rotation(angle axis), and translation [rX,rY,rZ,tx,ty,tz].
 *  - 3 => a 3D point data block.
 *
 */
struct ResidualErrorFunctor_Pinhole_Intrinsic_Radial_K1
{
  ResidualErrorFunctor_Pinhole_Intrinsic_Radial_K1(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }

  // Enum to map intrinsics parameters between openMVG & ceres camera data parameter block.
  enum {
    OFFSET_FOCAL_LENGTH = 0,
    OFFSET_PRINCIPAL_POINT_X = 1,
    OFFSET_PRINCIPAL_POINT_Y = 2,
    OFFSET_DISTO_K1 = 3
  };

  /**
   * @param[in] cam_K: Camera intrinsics( focal, principal point [x,y], K1 )
   * @param[in] cam_Rt: Camera parameterized using one block of 6 parameters [R;t]:
   *   - 3 for rotation(angle axis), 3 for translation
   * @param[in] pos_3dpoint
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_K,
    const T* const cam_Rt,
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    //--
    // Apply external parameters (Pose)
    //--

    const T * cam_R = cam_Rt;
    const T * cam_t = &cam_Rt[3];

    T pos_proj[3];
    // Rotate the point according the camera rotation
    ceres::AngleAxisRotatePoint(cam_R, pos_3dpoint, pos_proj);

    // Apply the camera translation
    pos_proj[0] += cam_t[0];
    pos_proj[1] += cam_t[1];
    pos_proj[2] += cam_t[2];

    // Transform the point from homogeneous to euclidean (undistorted point)
    const T x_u = pos_proj[0] / pos_proj[2];
    const T y_u = pos_proj[1] / pos_proj[2];

    //--
    // Apply intrinsic parameters
    //--

    const T& focal = cam_K[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = cam_K[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = cam_K[OFFSET_PRINCIPAL_POINT_Y];
    const T& k1 = cam_K[OFFSET_DISTO_K1];

    // Apply distortion (xd,yd) = disto(x_u,y_u)
    const T r2 = x_u*x_u + y_u*y_u;
    const T r_coeff = (T(1) + k1*r2);
    const T x_d = x_u * r_coeff;
    const T y_d = y_u * r_coeff;

    // Apply focal length and principal point to get the final image coordinates
    const T projected_x = principal_point_x + focal * x_d;
    const T projected_y = principal_point_y + focal * y_d;

    // Compute and return the error is the difference between the predicted
    //  and observed position
    out_residuals[0] = projected_x - T(m_pos_2dpoint[0]);
    out_residuals[1] = projected_y - T(m_pos_2dpoint[1]);

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

/**
 * @brief Ceres functor to use a Pinhole_Intrinsic_Radial_K3
 *
 *  Data parameter blocks are the following <2,6,6,3>
 *  - 2 => dimension of the residuals,
 *  - 6 => the intrinsic data block [focal, principal point x, principal point y, K1, K2, K3],
 *  - 6 => the camera extrinsic data block (camera orientation and position) [R;t],
 *         - rotation(angle axis), and translation [rX,rY,rZ,tx,ty,tz].
 *  - 3 => a 3D point data block.
 *
 */
struct ResidualErrorFunctor_Pinhole_Intrinsic_Radial_K3
{
  ResidualErrorFunctor_Pinhole_Intrinsic_Radial_K3(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }

  // Enum to map intrinsics parameters between openMVG & ceres camera data parameter block.
  enum {
    OFFSET_FOCAL_LENGTH = 0,
    OFFSET_PRINCIPAL_POINT_X = 1,
    OFFSET_PRINCIPAL_POINT_Y = 2,
    OFFSET_DISTO_K1 = 3,
    OFFSET_DISTO_K2 = 4,
    OFFSET_DISTO_K3 = 5,
  };

  /**
   * @param[in] cam_K: Camera intrinsics( focal, principal point [x,y], k1, k2, k3 )
   * @param[in] cam_Rt: Camera parameterized using one block of 6 parameters [R;t]:
   *   - 3 for rotation(angle axis), 3 for translation
   * @param[in] pos_3dpoint
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_K,
    const T* const cam_Rt,
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    //--
    // Apply external parameters (Pose)
    //--

    const T * cam_R = cam_Rt;
    const T * cam_t = &cam_Rt[3];

    T pos_proj[3];
    // Rotate the point according the camera rotation
    ceres::AngleAxisRotatePoint(cam_R, pos_3dpoint, pos_proj);

    // Apply the camera translation
    pos_proj[0] += cam_t[0];
    pos_proj[1] += cam_t[1];
    pos_proj[2] += cam_t[2];

    // Transform the point from homogeneous to euclidean (undistorted point)
    const T x_u = pos_proj[0] / pos_proj[2];
    const T y_u = pos_proj[1] / pos_proj[2];

    //--
    // Apply intrinsic parameters
    //--

    const T& focal = cam_K[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = cam_K[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = cam_K[OFFSET_PRINCIPAL_POINT_Y];
    const T& k1 = cam_K[OFFSET_DISTO_K1];
    const T& k2 = cam_K[OFFSET_DISTO_K2];
    const T& k3 = cam_K[OFFSET_DISTO_K3];

    // Apply distortion (xd,yd) = disto(x_u,y_u)
    const T r2 = x_u*x_u + y_u*y_u;
    const T r4 = r2 * r2;
    const T r6 = r4 * r2;
    const T r_coeff = (T(1) + k1*r2 + k2*r4 + k3*r6);
    const T x_d = x_u * r_coeff;
    const T y_d = y_u * r_coeff;

    // Apply focal length and principal point to get the final image coordinates
    const T projected_x = principal_point_x + focal * x_d;
    const T projected_y = principal_point_y + focal * y_d;

    // Compute and return the error is the difference between the predicted
    //  and observed position
    out_residuals[0] = projected_x - T(m_pos_2dpoint[0]);
    out_residuals[1] = projected_y - T(m_pos_2dpoint[1]);

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

/**
* @brief Ceres functor to use a Pinhole_Rig_Intrinsic(pinhole with subpose) and a 3D point.
*
*  Data parameter blocks are the following <2,9,6,3>
*  - 2 => dimension of the residuals,
*  - 9 => the intrinsic data block [focal, principal point x, principal point y, subpose[R;t]],
*  - 6 => the camera extrinsic data block (camera orientation and position) [R;t],
*         - rotation(angle axis), and translation [rX,rY,rZ,tx,ty,tz].
*  - 3 => a 3D point data block.
*
*/
struct ResidualErrorFunctor_Pinhole_Rig_Intrinsic
{
  ResidualErrorFunctor_Pinhole_Rig_Intrinsic(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }

  // Enum to map intrinsics parameters between openMVG & ceres camera data parameter block.
  enum {
    OFFSET_FOCAL_LENGTH = 0,
    OFFSET_PRINCIPAL_POINT_X = 1,
    OFFSET_PRINCIPAL_POINT_Y = 2
  };

  /**
  * @param[in] cam_K: Camera intrinsics( focal, principal point [x,y], subpose [R;t] )
  * @param[in] cam_Rt: Camera parameterized using one block of 6 parameters [R;t]:
  *   - 3 for rotation(angle axis), 3 for translation
  * @param[in] pos_3dpoint
  * @param[out] out_residuals
  */
  template <typename T>
  bool operator()(
    const T* const cam_K,
    const T* const cam_Rt,
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    //--
    // Apply external parameters (Pose)
    // Pose is obtained from pose combinaison: globalPose = subpose * pose = P_s * P
    // R R_s X + R t_s + t
    //--

    const T * cam_pose_R = cam_Rt;
    const T * cam_pose_t = &cam_Rt[3];

    const T * cam_subpose_R = &cam_K[3];
    const T * cam_subpose_t = &cam_K[6];

    T pos_rig[3];
    T pos_proj[3];
    T rig_trans[3];

    // R R_s X
    ceres::AngleAxisRotatePoint(cam_pose_R, pos_3dpoint, pos_rig);
    T pos_world[3];
    ceres::AngleAxisRotatePoint(cam_subpose_R, pos_rig, pos_proj);
    // R t_s
    T local_rig_trans[3];
    ceres::AngleAxisRotatePoint(cam_pose_R, cam_subpose_t, rig_trans);

    // Apply translations (R t_s + t)
    pos_proj[0] += cam_subpose_t[0] + rig_trans[0];
    pos_proj[1] += cam_subpose_t[1] + rig_trans[1];
    pos_proj[2] += cam_subpose_t[2] + rig_trans[2];

    // Transform the point from homogeneous to euclidean
    const T x_u = pos_proj[0] / pos_proj[2];
    const T y_u = pos_proj[1] / pos_proj[2];

    //--
    // Apply intrinsic parameters
    //--

    const T& focal = cam_K[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = cam_K[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = cam_K[OFFSET_PRINCIPAL_POINT_Y];

    // Apply focal length and principal point to get the final image coordinates
    const T projected_x = principal_point_x + focal * x_u;
    const T projected_y = principal_point_y + focal * y_u;

    // Compute and return the error is the difference between the predicted
    //  and observed position
    out_residuals[0] = projected_x - T(m_pos_2dpoint[0]);
    out_residuals[1] = projected_y - T(m_pos_2dpoint[1]);

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

} // namespace sfm
} // namespace openMVG

#endif // OPENMVG_SFM_DATA_BA_CERES_CAMERA_FUNCTOR_HPP
