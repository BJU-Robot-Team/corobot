#include <ros/ros.h>
#include <corobot_msgs/GPSFix.h>
#include <corobot_msgs/GPSStatus.h>
#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/NavSatStatus.h>
#include <libgpsmm.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>

using namespace corobot_msgs;
using namespace sensor_msgs;

class GPSDClient {
//Class that does the interface with the GPS device
  public:
    //class constructor
    GPSDClient() : privnode("~"), use_gps_time(true) {
#if GPSD_API_MAJOR_VERSION < 5 // We need different methods of doing depending on the version of the gpsd api
    gps = new gpsmm();
    gps_state = 0;
#endif
    }

    ~GPSDClient()
    {
	delete gps;
    }

    bool start() 
    /**
     * Function that innitialize ros topics and parameters, as well as initialize the gps device.
     */
     { 
      gps_fix_pub = node.advertise<GPSFix>("extended_fix", 1); //declare topic with gps information. This is the extended version of the message and includes more data than the non-extended version.
      navsat_fix_pub = node.advertise<NavSatFix>("fix", 1); //topic that send gps information

      privnode.getParam("use_gps_time", use_gps_time); // decide if we use the gps time or the ros time for the timestamp of the messages

 	// We need to know where to get the data from gpsd
      std::string host = "localhost"; 
      int port = 2947;
      privnode.getParam("host", host);
      privnode.getParam("port", port);

      char port_s[12];
      snprintf(port_s, 12, "%d", port);

	//open the gps device
      gps_data_t *resp;
#if GPSD_API_MAJOR_VERSION >= 5
    gps = new gpsmm(host.c_str(), port_s);
#elif GPSD_API_MAJOR_VERSION < 5
      resp = gps->open(host.c_str(), port_s);
      if (resp == NULL) {
        ROS_ERROR("Failed to open GPSd");
	gps_state = 1; // used for diagnostics
        return false;
      }
#endif
	// Again we need to differentiate the different gpsd api versions
#if GPSD_API_MAJOR_VERSION >= 5
      resp = gps->stream(WATCH_ENABLE);
      if (resp == NULL) {
        ROS_ERROR("Failed to intialize the gps");
	gps_state = 1;
	return false;
      }

#elif GPSD_API_MAJOR_VERSION == 4
      resp = gps->stream(WATCH_ENABLE);
      if (resp == NULL) {
        ROS_ERROR("Failed to intialize the gps");
	gps_state = 1;
	return false;
      }
#elif GPSD_API_MAJOR_VERSION == 3
      gps->query("w\n");
#else
#error "gpsd_client only supports gpsd API versions 3, 4 and 5"
      gps_state = 2;
#endif
      

      ROS_INFO("GPSd opened");
      return true;
    }

    void step() {
	/**
	 * read the gps data and call the function to process it
	 */
      gps_data_t *p;
#if GPSD_API_MAJOR_VERSION >= 5
      gps_read(p);
#elif GPSD_API_MAJOR_VERSION < 5
      p = gps->poll();
#endif
      process_data(p);
    }

    void stop() {
      // gpsmm doesn't have a close method? OK ...
    }

    void gps_diagnostic(diagnostic_updater::DiagnosticStatusWrapper &stat)
    /**
     * Function that will report the status of the hardware to the diagnostic topic
     */
    {
	if (gps_state == 0)  
		stat.summaryf(diagnostic_msgs::DiagnosticStatus::OK, "The gps is working");
	else if (gps_state == 1)
	{
		stat.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR, "Can't intialize");
		stat.addf("Recommendation", "The gps could not be initialized. Please make sure the gps is connected to the motherboard and is configured. You can follow points 1.3 and after in the following tutorial for the configuration: http://ros.org/wiki/gpsd_client/Tutorials/Getting started with gpsd_client");
	}
	else if (gps_state == 2)
	{
		stat.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR, "wrong lib gpsd version");
		stat.addf("Recommendation", "Please make sure that gpsd is installed and that you have at least the api major version 3 or after installed.");
	}
    }

  private:
    ros::NodeHandle node;
    ros::NodeHandle privnode;
    ros::Publisher gps_fix_pub; 
    ros::Publisher navsat_fix_pub;
    gpsmm *gps; // variable that permits us to read gps data
    int gps_state;

    bool use_gps_time;

    void process_data(struct gps_data_t* p) 
	/**
	 * filter the data if none or if the gps is offline, then call the process data function
	 */
	{
      if (p == NULL)
        return;

      if (!p->online)
        return;

      process_data_gps(p);
      process_data_navsat(p);
    }

#if GPSD_API_MAJOR_VERSION >= 4
#define SATS_VISIBLE p->satellites_visible
#elif GPSD_API_MAJOR_VERSION == 3
#define SATS_VISIBLE p->satellites
#endif

    void process_data_gps(struct gps_data_t* p) 
	/** 
	 * Process the gps data and send the extended message version
	 */
	{
      ros::Time time = ros::Time::now();

      GPSFix fix; //ros message
      GPSStatus status; //ros message

      status.header.stamp = time;
      fix.header.stamp = time;

	// fill in the gps status messages parameters, which is a parameter of the GPSFix message
      status.satellites_used = p->satellites_used;

      status.satellite_used_prn.resize(status.satellites_used);
      for (int i = 0; i < status.satellites_used; ++i) {
        status.satellite_used_prn[i] = p->used[i];
      }

      status.satellites_visible = SATS_VISIBLE;

      status.satellite_visible_prn.resize(status.satellites_visible);
      status.satellite_visible_z.resize(status.satellites_visible);
      status.satellite_visible_azimuth.resize(status.satellites_visible);
      status.satellite_visible_snr.resize(status.satellites_visible);

      for (int i = 0; i < SATS_VISIBLE; ++i) {
        status.satellite_visible_prn[i] = p->PRN[i];
        status.satellite_visible_z[i] = p->elevation[i];
        status.satellite_visible_azimuth[i] = p->azimuth[i];
        status.satellite_visible_snr[i] = p->ss[i];
      }

      if ((p->status & STATUS_FIX) && !isnan(p->fix.epx)) {
        status.status = 0; // FIXME: gpsmm puts its constants in the global
                           // namespace, so `GPSStatus::STATUS_FIX' is illegal.

        if (p->status & STATUS_DGPS_FIX)
          status.status |= 18; // same here

	// fill in all the fix messages parameters
        fix.time = p->fix.time;
        fix.latitude = p->fix.latitude;
        fix.longitude = p->fix.longitude;
        fix.altitude = p->fix.altitude;
        fix.track = p->fix.track;
        fix.speed = p->fix.speed;
        fix.climb = p->fix.climb;

#if GPSD_API_MAJOR_VERSION > 3
        fix.pdop = p->dop.pdop;
        fix.hdop = p->dop.hdop;
        fix.vdop = p->dop.vdop;
        fix.tdop = p->dop.tdop;
        fix.gdop = p->dop.gdop;
#else
        fix.pdop = p->pdop;
        fix.hdop = p->hdop;
        fix.vdop = p->vdop;
        fix.tdop = p->tdop;
        fix.gdop = p->gdop;
#endif

        fix.err = p->epe;
        fix.err_vert = p->fix.epv;
        fix.err_track = p->fix.epd;
        fix.err_speed = p->fix.eps;
        fix.err_climb = p->fix.epc;
        fix.err_time = p->fix.ept;

        /* TODO: attitude */
      } else {
      	status.status = -1; // STATUS_NO_FIX
      }

      fix.status = status;

      gps_fix_pub.publish(fix);
    }

    void process_data_navsat(struct gps_data_t* p) 
	/** 
	 * Process the gps data and send the short message version
	 */
{
      NavSatFixPtr fix(new NavSatFix);

      /* TODO: Support SBAS and other GBAS. */

      if (use_gps_time)
        fix->header.stamp = ros::Time(p->fix.time);
      else
        fix->header.stamp = ros::Time::now();

      /* gpsmm pollutes the global namespace with STATUS_,
       * so we need to use the ROS message's integer values
       * for status.status
       */
      switch (p->status) {
        case STATUS_NO_FIX:
          fix->status.status = -1; // NavSatStatus::STATUS_NO_FIX;
          break;
        case STATUS_FIX:
          fix->status.status = 0; // NavSatStatus::STATUS_FIX;
          break;
        case STATUS_DGPS_FIX:
          fix->status.status = 2; // NavSatStatus::STATUS_GBAS_FIX;
          break;
      }

      fix->status.service = NavSatStatus::SERVICE_GPS;

      fix->latitude = p->fix.latitude;
      fix->longitude = p->fix.longitude;
      fix->altitude = p->fix.altitude;

      /* gpsd reports status=OK even when there is no current fix,
       * as long as there has been a fix previously. Throw out these
       * fake results, which have NaN variance
       */
      if (isnan(p->fix.epx)) {
        //return;    //MODIFIED
      }

      fix->position_covariance[0] = p->fix.epx;
      fix->position_covariance[4] = p->fix.epy;
      fix->position_covariance[8] = p->fix.epv;

      fix->position_covariance_type = NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;

      navsat_fix_pub.publish(fix);
    }
};

int main(int argc, char ** argv) {
  ros::init(argc, argv, "corobot_gps");

  GPSDClient client;


  //create an updater that will send information on the diagnostics topics
  diagnostic_updater::Updater updater;
  updater.setHardwareIDf("GPS");
  updater.add("gps", &client, &GPSDClient::gps_diagnostic); //function that will be executed with updater.update()

  if (!client.start()) // We stop the program if the gps did not initialize
  {
    updater.force_update();
    return -1;
  }


  while(ros::ok()) {
    ros::spinOnce();
    updater.update();
    client.step();
  }

  client.stop();
}
