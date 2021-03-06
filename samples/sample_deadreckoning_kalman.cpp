#include "ardrone/ardrone.h"

// --------------------------------------------------------------------------
// main(Number of arguments, Argument values)
// Description  : This is the entry point of the program.
// Return value : SUCCESS:0  ERROR:-1
// --------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // AR.Drone class
    ARDrone ardrone;

    // Initialize
    if (!ardrone.open()) {
        printf("Failed to initialize.\n");
        return -1;
    }

    // Battery
    printf("Battery = %d%%\n", ardrone.getBatteryPercentage());

    // Map
    cv::Mat map = cv::Mat::zeros(500, 500, CV_8UC3);

    // Kalman filter
    cv::KalmanFilter kalman(6, 4, 0);

    // Transition matrix (x, y, z, vx, vy, vz)
    cv::Mat1f A(6, 6);
    A << 1, 0, 0, 1, 0, 0,
         0, 1, 0, 0, 1, 0,
         0, 0, 1, 0, 0, 1,
         0, 0, 0, 1, 0, 0,
         0, 0, 0, 0, 1, 0,
         0, 0, 0, 0, 0, 1;
    kalman.transitionMatrix = A;

    // Measurement matrix (0, 0, z, vx, vy, vz)
    cv::Mat1f H(4, 6);
    H << 0, 0, 1, 0, 0, 0,
         0, 0, 0, 1, 0, 0,
         0, 0, 0, 0, 1, 0;
         0, 0, 0, 0, 0, 1;
    kalman.measurementMatrix = H;

    // Error covariances
    cv::setIdentity(kalman.processNoiseCov,     cv::Scalar::all(1e-4));
    cv::setIdentity(kalman.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(kalman.errorCovPost,        cv::Scalar::all(1e-2));

    // Main loop
    while (1) {
        // Key input
        int key = cv::waitKey(30);
        if (key == 0x1b) break;

        // Update
        if (!ardrone.update()) break;

        // Get an image
        cv::Mat image = cv::Mat(ardrone.getImage());

        // Prediction
        cv::Mat prediction = kalman.predict();

        // Altitude
        double altitude = ardrone.getAltitude();

        // Orientations
        double roll  = ardrone.getRoll();
        double pitch = ardrone.getPitch();
        double yaw   = ardrone.getYaw();

        // Velocities
        double vx, vy, vz;
        double velocity = ardrone.getVelocity(&vx, &vy, &vz);
        cv::Mat V  = (cv::Mat1f(3,1) << vx, vy, vz);

        // Rotation matrices
        cv::Mat RZ = (cv::Mat1f(3,3) <<   cos(yaw), -sin(yaw),        0.0,
                                          sin(yaw),  cos(yaw),        0.0,
                                               0.0,       0.0,        1.0);
        cv::Mat RY = (cv::Mat1f(3,3) << cos(pitch),       0.0,  sin(pitch),
                                               0.0,       1.0,        0.0,
                                       -sin(pitch),       0.0,  cos(pitch));
        cv::Mat RX = (cv::Mat1f(3,3) <<        1.0,       0.0,        0.0,
                                               0.0, cos(roll), -sin(roll),
                                               0.0, sin(roll),  cos(roll));

        // Time [s]
        static int64 last = cv::getTickCount();
        double dt = (cv::getTickCount() - last) / cv::getTickFrequency();
        last = cv::getTickCount();

        // Local movements (z, vx, vy, vz)
        cv::Mat1f M = RZ * RY * RX * V * dt;
        cv::Mat measurement = (cv::Mat1f(4,1) << altitude, M(0,0), M(1,0), M(2,0));

        // Correction
        cv::Mat1f estimated = kalman.correct(measurement);

        // Position (x, y, z)
        double pos[3] = {estimated(0,0), estimated(1,0), estimated(2,0)};
        printf("x = %3.2fm, y = %3.2fm, z = %3.2fm\n", pos[0], pos[1], pos[2]);

        // Take off / Landing 
        if (key == ' ') {
            if (ardrone.onGround()) ardrone.takeoff();
            else                    ardrone.landing();
        }

        // Move
        double x = 0.0, y = 0.0, z = 0.0, r = 0.0;
        if (key == 0x260000) x =  1.0;
        if (key == 0x280000) x = -1.0;
        if (key == 0x250000) r =  1.0;
        if (key == 0x270000) r = -1.0;
        if (key == 'q')      z =  1.0;
        if (key == 'a')      z = -1.0;
        ardrone.move3D(x, y, z, r);

        // Change camera
        static int mode = 0;
        if (key == 'c') ardrone.setCamera(++mode%4);

        // Display the image
        cv::circle(map, cv::Point(-pos[1]*100.0 + map.cols/2, -pos[0]*100.0 + map.rows/2), 2, CV_RGB(255,0,0));
        cv::imshow("map", map);
        cv::imshow("camera", image);
    }

    // See you
    ardrone.close();

    return 0;
}