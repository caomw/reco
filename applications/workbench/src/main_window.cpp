/*
 * main_window.cpp
 *
 *  Created on: Dec 17, 2014
 *      Author: Gregory Kramida
 *     License: Apache v2
 *   Copyright: (c) Gregory Kramida 2014
 */

//local
#include <src/main_window.h>
#include "ui_main_window.h"

//datapipe
#include <reco/datapipe/kinect_v2_info.h>
#include <reco/datapipe/freenect2_pipe.h>
#include <QThread>
#include <QDebug>
#include <QFileDialog>

// Point Cloud Library
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/visualization/pcl_visualizer.h>

// Visualization Toolkit (VTK)
#include <vtkRenderWindow.h>
#include <QVTKWidget.h>

// ARPG includes
#include <calibu/Calibu.h>
#include <HAL/Camera/CameraDevice.h>

//OpenCV Includes
#include <opencv2/core/eigen.hpp>

//utils
#include <reco/utils/cpp_exception_util.h>
#include <reco/utils/debug_util.h>
#include <reco/utils/color_util.h>

namespace reco {
namespace workbench {

#define CAMERA_PX_WIDTH 1920
#define CAMERA_PX_HEIGHT 1080
#define DEFAULT_CALIBRATION_FILE_PATH "media/algomorph/Data/reco/calib/pos_D_2_kinects.xml"
#define DEFAULT_LOG_FILE_PATH "/media/algomorph/Data/reco/cap/pos_D_slow_rotating_human_2_kinects_1240_frames.log"
//#define DEFAULT_LOG_FILE_PATH "/media/algomorph/Data/reco/cap/pos_E_moving_human_4_kinects.log"

main_window::main_window() :
		ui(new Ui_main_window),
				rgb_viewer("RGB Feed", NULL),
				buffer(
						new utils::optimistic_assignment_swap_buffer<
								std::shared_ptr<hal::ImageArray>>()),
				pipe(
						new datapipe::freenect2_pipe(buffer, datapipe::freenect2_pipe::hal_log,
								DEFAULT_LOG_FILE_PATH)),
				cloud(new pcl::PointCloud<pcl::PointXYZRGB>)
{
	ui->setupUi(this);

	// Set up the QVTK window
	result_viewer.reset(new pcl::visualization::PCLVisualizer("result view", false));
	result_viewer->setCameraPosition(
			0.0, 0.0, 0.0,   // camera position
			0.0, 0.0, 1.0,   // viewpoint
			0.0, -1.0, 0.0,  // normal
			0.0);            // viewport


	ui->qvtk_widget->SetRenderWindow(result_viewer->getRenderWindow());
	result_viewer->setupInteractor(ui->qvtk_widget->GetInteractor(),
			ui->qvtk_widget->GetRenderWindow());
	ui->qvtk_widget->update();

	connect_actions();

	hook_pipe_signals();//start pipe with default file
}

main_window::~main_window() {
	delete ui;

}

/**
 * Connect actions of menus with the corresponding slot functions
 */

void main_window::connect_actions() {
	connect(ui->action_open_kinect_devices, SIGNAL(triggered()), this, SLOT(open_kinect_devices()));
	connect(ui->action_open_hal_log, SIGNAL(triggered()), this, SLOT(open_hal_log()));
	connect(ui->action_open_image_folder, SIGNAL(triggered()), this, SLOT(open_image_folder()));
	connect(ui->action_open_calibration_file, SIGNAL(triggered()),this, SLOT(open_calibration_file()));
	connect(ui->action_close_stream, SIGNAL(triggered()), this, SLOT(unhook_pipe_signals()));
}

/**
 * Open kinect feed source from actual devices (if possible)
 */
void main_window::open_kinect_devices() {
	unhook_pipe_signals();
	pipe.reset(new datapipe::freenect2_pipe(buffer, datapipe::freenect2_pipe::kinect2_device));
	hook_pipe_signals();
}
/**
 * Open kinect feed source from hal log file
 */
void main_window::open_hal_log() {
	QString file_name = QFileDialog::getOpenFileName(this, tr("Open Log File"),
			"/media/algomorph/Data/reco/cap/", tr("HAL Log files (*.log)"));

	if (!file_name.isEmpty()) {
		unhook_pipe_signals();
		//TODO: test if QString --> std::string works on windows like this
		pipe.reset(
				new datapipe::freenect2_pipe(buffer, datapipe::freenect2_pipe::hal_log,
						file_name.toStdString()));
		hook_pipe_signals();

	}
}

/**
 * Check to see if both the calibration file has been loaded and the pipe signals have been hooked,
 * if so: enable the reconstruction processing & playback controls
 */
void main_window::toggle_reco_controls() {
	if (this->calibration_loaded && this->pipe_signals_hooked) {
		this->ui->reco_proc_group->setEnabled(true);
		this->ui->reco_playback_group->setEnabled(true);
	}else{
		this->ui->reco_proc_group->setEnabled(false);
		this->ui->reco_playback_group->setEnabled(false);
	}
}


/**
 * Open kinect feed from image folder
 */
void main_window::open_image_folder() {
	throw reco::utils::not_implemented();
}

/**
 * Load the calibration from file at the given path
 * @param file_path the given path
 */
void main_window::open_calibration_file() {
	QString qfile_path = QFileDialog::getOpenFileName(this, tr("Open Calibration File"),
			"/media/algomorph/Data/reco/calib/", tr("Calibu calibration files (*.xml)"));

	if (pipe_signals_hooked && !qfile_path.isEmpty()) {
		std::string file_path = qfile_path.toStdString();
		//parse intrinsics
		std::shared_ptr<calibu::Rigd> rig = calibu::ReadXmlRig(file_path);

		//to aviod magic numbers
		const int n_channels_per_kinect = datapipe::kinect_v2_info::channels.size();
		const int num_kinects = pipe->get_num_kinects();
		const int depth_offset = datapipe::kinect_v2_info::depth_channel.offset();

		//check against the pipe's number of channels
		if ((int)rig->cameras_.size() != pipe->get_num_channels()) {
			err(std::invalid_argument)
					<< "The number of kinect feeds in the provided calibration file ("
					<< rig->cameras_.size() / n_channels_per_kinect
					<< ") does not correspond to the number of kinect feeds in the provided log file (presumably, "
					<< num_kinects << ")." << enderr;
		}
		for (int i_kinect = 0; i_kinect < num_kinects; i_kinect++) {
			//pick out the depth cam matrix
			Eigen::Matrix3f cam_model = rig->cameras_[depth_offset
					+ i_kinect * n_channels_per_kinect]->K().cast<float>();
			//convert to opencv matrix
			//TODO: possibly get rid of the need to convert to OpenCV
			cv::Mat K_depth(3, 3, CV_32F);
			cv::eigen2cv(cam_model, K_depth);

			//store intrinsics for future use
			depth_intrinsics.push_back(K_depth);
			depth_rotations.push_back(
					rig->cameras_[depth_offset + i_kinect * n_channels_per_kinect]->Pose().rotationMatrix().cast<
							float>());
			Eigen::Vector3f translation =
					rig->cameras_[depth_offset + i_kinect * n_channels_per_kinect]->Pose().translation().cast<
							float>().col(0);
			depth_translations.push_back(translation);
		}
		calibration_loaded = true;
		toggle_reco_controls();
	}
}

/**
 * Connect pipe signals from output and GUI buttons to pipe slots
 */
void main_window::hook_pipe_signals() {
	this->unhook_pipe_signals();
	//set up error reporting;
	connect(pipe.get(), SIGNAL(error(QString)), this, SLOT(report_error(QString)));
	//connect the play and pause buttons
	connect(ui->pause_button, SIGNAL(released()), pipe.get(), SLOT(pause()));
	connect(ui->play_button, SIGNAL(released()), pipe.get(), SLOT(play()));
	//connect the pipe output to viewer
	connect(pipe.get(), SIGNAL(frame()), this,
			SLOT(display_feeds()));
	rgb_viewer.configure_for_pipe(pipe->get_num_channels());
	depth_viewer.configure_for_pipe(pipe->get_num_channels());

	cloud_colors.clear();
	cloud_colors.reserve(pipe->get_num_kinects());
	for(int i_kinect = 0; i_kinect < pipe->get_num_kinects(); i_kinect++){
		cloud_colors.push_back(utils::generate_random_color());
	}

	pipe_signals_hooked = true; //set flag
}

/**
 * Disconnect pipe signals between output/GUI buttons and pipe slots
 */
void main_window::unhook_pipe_signals() {
	if (pipe_signals_hooked) {
		//disconnect the viewer windows from the pipe
		disconnect(pipe.get(), 0, 0, 0);
		//disconnect the pipe from the play, pause buttons
		disconnect(ui->play_button, 0, 0, 0);
		disconnect(ui->pause_button, 0, 0, 0);
		//reset the viewers
		this->rgb_viewer.clear_gui_configuration();
		this->depth_viewer.clear_gui_configuration();
		//------
		shut_pipe_down();
		//unset flags
		pipe_signals_hooked = false;
		calibration_loaded = false;
		toggle_reco_controls();


	}
}
/**
 * Shut down the RGBD video source pipe permanently
 */
void main_window::shut_pipe_down() {
	pipe->stop();
	buffer->clear();		//let one more item onto the queue
	pipe->join_thread();
}

/**
 * Displays the current frame from the buffer on the screen
 */
void main_window::display_feeds() {
	std::shared_ptr<hal::ImageArray> images = this->buffer->pop_front();
	this->rgb_viewer.on_frame(images);
	this->depth_viewer.on_frame(images);
}

/**
 * Slot for error reporting (eventually, errors emanating from child threads should print the error
 * to stdout or stderr)
 * @param string - error message
 */
void main_window::report_error(QString string) {
	qDebug() << string;
}

/**
 * On window close, close the extra windows and shut down data transfer
 * @param event window close event
 */
void main_window::closeEvent(QCloseEvent* event) {
	rgb_viewer.close();
	depth_viewer.close();
	if (pipe_signals_hooked) {
		shut_pipe_down();
	}
}

void main_window::on_show_rgb_feed_button_clicked() {
	this->rgb_viewer.setVisible(true);
}
void main_window::on_show_depth_feed_button_clicked() {
	this->depth_viewer.setVisible(true);
}

} //end namespace reco
} //end namespace workbench
