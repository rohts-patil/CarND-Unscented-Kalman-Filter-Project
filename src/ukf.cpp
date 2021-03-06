#include <iostream>
#include "ukf.h"

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {

  is_initialized_ = false;

  previous_timestamp_ = 0;

  // if this is false, laser measurements will be ignored (except during init)
  use_lidar_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.8;  //std_a_ = 30;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.7; //std_yawdd_ = 30;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;     // 0.1

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;  //0.0175

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  n_x_ = 5;

  n_aug_ = 7;

  n_aug_sigma_ = 2 * n_aug_ + 1;

  lambda_ = 3 - n_aug_;
  
  Xsig_aug_ = MatrixXd(n_aug_, n_aug_sigma_);

  Xsig_pred_ = MatrixXd(n_x_, n_aug_sigma_);

  weights_ = VectorXd(n_aug_sigma_);

  double weight_0 = lambda_ / (lambda_ + n_aug_);
  
  weights_(0) = weight_0;

  for (int i = 1; i < n_aug_sigma_; ++i) {
    double weight = 0.5 / (n_aug_ + lambda_);
    weights_(i) = weight;
  }

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */


  if (!is_initialized_) {
    
    
    P_ << 1, 0, 0, 0, 0,
          0, 1, 0, 0, 0,
          0, 0, 1, 0, 0,
          0, 0, 0, 1, 0,
          0, 0, 0, 0, 1;

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      
      double rho = meas_package.raw_measurements_[0];
      double phi = meas_package.raw_measurements_[1];
      double px = rho * cos(phi);
      double py = rho * sin(phi);
      x_ << px, py, 0, 0, 0;

    } else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      
      double px = meas_package.raw_measurements_[0];
      double py = meas_package.raw_measurements_[1];
      x_ << px, py, 0, 0, 0;

    }

    previous_timestamp_ = meas_package.timestamp_;
    
    
    is_initialized_ = true;
    cout << "***********************Starting and Initializing*******************" <<endl;
    cout << "x:" << endl << x_<< endl; 
    cout << "P:" << endl << P_<< endl;
    return;

  }

  if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_lidar_ == false)
    return;
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_ == false)
    return;

  double dt = (meas_package.timestamp_ - previous_timestamp_) / 1000000.0;
  previous_timestamp_ = meas_package.timestamp_;


  while (dt > 0.1) {
    Prediction(0.05);
    dt = dt - 0.05;
  }

  Prediction(dt);

  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    UpdateRadar(meas_package);

  } else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
    UpdateLidar(meas_package);
  }

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  MatrixXd Xsig_aug(n_aug_, n_aug_sigma_);
  AugmentedSigmaPoints(x_, P_, Xsig_aug);
  SigmaPointPrediction(delta_t, Xsig_aug, Xsig_pred_);
  PredictMeanAndCovariance(Xsig_pred_, x_, P_);

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  int n_z = meas_package.raw_measurements_.rows();

  VectorXd z_pred(n_z);
  MatrixXd S(n_z, n_z);
  MatrixXd Zsig(n_z, n_aug_sigma_);

  PredictLidarMeasurement(Xsig_pred_, Zsig, z_pred, S);
  UpdateState(meas_package.raw_measurements_, z_pred, S, Xsig_pred_, Zsig, x_, P_);

  VectorXd z_diff = meas_package.raw_measurements_-z_pred;
  NIS_lidar_ = z_diff.transpose() * S.inverse() * z_diff;

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  int n_z = meas_package.raw_measurements_.rows();

  VectorXd z_pred(n_z);
  MatrixXd S(n_z, n_z);
  MatrixXd Zsig(n_z, n_aug_sigma_);

  PredictRadarMeasurement(Xsig_pred_, Zsig, z_pred, S);
  UpdateState(meas_package.raw_measurements_, z_pred, S, Xsig_pred_, Zsig, x_, P_);

  VectorXd z_diff = meas_package.raw_measurements_-z_pred;

  while (z_diff(2)> M_PI) z_diff(2)-=2.*M_PI;
  while (z_diff(2)<-M_PI) z_diff(2)+=2.*M_PI;

  NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;

}


void UKF::AugmentedSigmaPoints(const VectorXd &x, const MatrixXd &P, MatrixXd &Xsig_aug) {

 
  VectorXd x_aug = VectorXd(n_aug_);

  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

  x_aug.head(5) = x;
  x_aug(5) = 0;
  x_aug(6) = 0;

  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_, n_x_) = P;
  P_aug(n_aug_-2, n_aug_-2) = std_a_ * std_a_;
  P_aug(n_aug_-1, n_aug_-1) = std_yawdd_ * std_yawdd_;


  Eigen::LLT<MatrixXd> lltOfPaug(P_aug);


  if (lltOfPaug.info() == Eigen::NumericalIssue) {

    cout << "LLTPaug failed!" << endl;
    throw range_error("LLTPaug failed");
  }
  
  MatrixXd L = lltOfPaug.matrixL();

  Xsig_aug.col(0)  = x_aug;

  
  for (int i = 0; i < n_aug_; i++)
  {
    Xsig_aug.col(i+1)         = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_)  = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

}

void UKF::SigmaPointPrediction(double delta_t, const MatrixXd &Xsig_aug, MatrixXd &Xsig_pred) {
  
  for (int i = 0; i< n_aug_sigma_; i++)
  {
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    double px_p, py_p;

    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin(yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
    py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
    v_p = v_p + nu_a * delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;
	
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }

}


void UKF::PredictMeanAndCovariance(const MatrixXd &Xsig_pred, VectorXd &x, MatrixXd &P) {

  x.fill(0.0);


  for (int i = 0; i < n_aug_sigma_; i++) {  
    x = x + weights_(i) * Xsig_pred.col(i);
  }


  P.fill(0.0);

  for (int i = 0; i < n_aug_sigma_; i++) {  

   
    VectorXd x_diff = Xsig_pred.col(i) - x;
   
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P = P + weights_(i) * x_diff * x_diff.transpose() ;
  }


}

void UKF::PredictRadarMeasurement(const MatrixXd &Xsig_pred, MatrixXd &Zsig, VectorXd &z_pred, MatrixXd &S) {

  
  int n_z = z_pred.rows();

 
  for (int i = 0; i < n_aug_sigma_; i++) {  

    
    double p_x = Xsig_pred(0,i);
    double p_y = Xsig_pred(1,i);
    double v   = Xsig_pred(2,i);
    double yaw = Xsig_pred(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        
    Zsig(1,i) = atan2(p_y, p_x);                                
    
    if (Zsig(0,i) != 0) {
      Zsig(2,i) = (p_x*v1 + p_y*v2) / Zsig(0,i);   
    } else {
      Zsig(2,i) = 0;
    }
  }


  z_pred.fill(0.0);

  for (int i=0; i < n_aug_sigma_; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  S.fill(0.0);

  for (int i = 0; i < n_aug_sigma_; i++) {  
    VectorXd z_diff = Zsig.col(i) - z_pred;
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_radr_*std_radr_,             0,                     0,
                 0,            std_radphi_*std_radphi_,           0,
                 0,                        0,           std_radrd_*std_radrd_;
  S = S + R;

}

void UKF::PredictLidarMeasurement(const MatrixXd &Xsig_pred, MatrixXd &Zsig, VectorXd &z_pred, MatrixXd &S) {

  int n_z = z_pred.rows();
  
  for (int i = 0; i < n_aug_sigma_; i++) {  
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    Zsig(0,i) = p_x;                       
    Zsig(1,i) = p_y;                                 
  }
  z_pred.fill(0.0);

  for (int i=0; i < n_aug_sigma_; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  S.fill(0.0);

  for (int i = 0; i < n_aug_sigma_; i++) { 
    VectorXd z_diff = Zsig.col(i) - z_pred;
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_laspx_*std_laspx_,         0,
                0,           std_laspy_*std_laspy_;
  
  S = S + R;
}


void UKF::UpdateState(const VectorXd &z, const VectorXd &z_pred, const MatrixXd &S,
                      const MatrixXd &Xsig_pred, const MatrixXd &Zsig, VectorXd &x, MatrixXd &P) {
  
  int n_z = z_pred.rows();
  int n_x = x.rows();

  MatrixXd Tc = MatrixXd(n_x, n_z);
  Tc.fill(0.0);
  for (int i = 0; i < n_aug_sigma_; i++) { 

    VectorXd z_diff = Zsig.col(i) - z_pred;
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    VectorXd x_diff = Xsig_pred.col(i) - x;
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  
  MatrixXd K = Tc * S.inverse();
  VectorXd z_diff = z - z_pred;
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  x = x + K * z_diff;
  P = P - K*S*K.transpose();

}



