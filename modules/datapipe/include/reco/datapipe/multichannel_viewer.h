/*
 * feed_viewer.h
 *
 *     Authors: Gregory Kramida
 *     License: Apache v. 2
 *   Copyright: (c) Gregory Kramida 2015 
 */

#ifndef RECO_DATAPIPE_MULTICHANNEL_VIEWER_H_
#define RECO_DATAPIPE_MULTICHANNEL_VIEWER_H_
#pragma once

//Qt
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

//datapipe
#include <reco/datapipe/image_widget.h>
#include <reco/datapipe/kinect_v2_info.h>
#include <reco/datapipe/typedefs.h>

//HAL
#include <HAL/Messages/ImageArray.h>



namespace reco {
namespace datapipe {

/**
 * A Qt widget for displaying multiple video channels at once.
 */
class multichannel_viewer: public QWidget {
Q_OBJECT
	private:
	QLayout* layout = new QVBoxLayout();
	QLabel* no_source_connected_label = new QLabel();
	bool configured_for_pipe = false;
	void add_video_widget(int ix_feed);


protected:
	std::vector<std::tuple<int, datapipe::image_widget*>> video_widgets;
	/**
	 * @brief returns a selection of channels
	 * @param num_channels total number of channels in the feed
	 * @return
	 */
	virtual std::vector<int> select_channels(int num_channels);

public:

	/**
	 * @brief primary constructor
	 * @param window_title what to set the title of the window is this widget is shown in a separate window
	 * @param parent the widget's parent widget
	 */
	multichannel_viewer(QWidget* parent = NULL, QString window_title = "Feed Viewer");

	virtual ~multichannel_viewer();
	/**
	 * @brief Hooks current object to show feed from the desired pipe
	 * @param num_channels total number of channels in the feed
	 */
	void set_channel_number(int num_channels);
	/**
	 * @brief Set all outputs to blank images of the specified width & height
	 * @param width
	 * @param height
	 */
	void set_blank(int width, int height);
	/**
	 * @brief Unhooks current object from the previously-connected pipe
	 */
	void clear_gui_configuration();
public slots:

	/**
	 * @brief triggered when a new frame becomes available.
	 * @param images
	 */
	virtual void on_frame(std::shared_ptr<hal::ImageArray> images);
	virtual void on_frame(std::shared_ptr<std::vector<cv::Mat>> images);

};

} /* namespace datapipe */
} /* namespace reco */

#endif /* RECO_DATAPIPE_MULTICHANNEL_VIEWER_H_ */
