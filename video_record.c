// This file consist of gstreamer c code for the 'Developing video record/Playback using GStreamer on TurboX-C610' project // 
 
#include<signal.h> 
#include<unistd.h>
#include"video_record.h"

int status;
GstElement *pipeline;
int retpid = -1;
APP_CONFIG config;

int prog;
void sigintHandler(int unused)
{
        // to handle interrupt signal from user // 
	retpid = fork(); 
    if(retpid==0)
	{

           if(prog == 0)
			{
            char *programName = "./publish";   
			char *args[] = {programName, "--device", "QCS610", "--recording",config.name,"--action","IDLE", "--client_id", "basicPubSub", NULL};
            execvp(programName, args);
           
}		   else if(prog == 1)
           {
            char *programName = "python3";   
			char *args[] = {programName, "basicPubSub.py", "-D", "QCS610", "-R",config.name,"-A","IDLE", NULL};
            execvp(programName, args);
           }
         //system("python3 basicPubSub.py -R 4K -D QCS610 -A STOP_CAPTURING");
    }
     waitpid(retpid, &status, 0);
    if (WIFSIGNALED(status)){
        g_print("error\n");
    }
    else if (WEXITSTATUS(status)){
        g_print("executed normally\n");
    }   
 
	g_print("Sending EOS stream to the pipeline\n");
	gst_element_send_event(pipeline, gst_event_new_eos());

	g_print("pipeline got EOS stream\n");
	
    
}


int main(int argc, char *argv[])
{	
	
	if(argc < 3)
	{
		g_print("enter required number of arguments\n");
		return 0;
	}
	

	
	if (memcmp(argv[1] ,"4k", 2) == 0)                                                        // for recording 4k video
	{	
		 set_config(&config, WIDTH_4K, HEIGHT_4K, "mp4mux",argv[1],"filesink",argv[2],"4K");
	}
	else if(memcmp(argv[1] ,"1080p", 5) == 0)                                                 // for recording 1080p video
	{	
		 set_config(&config, WIDTH_1K, HEIGHT_1K, "mp4mux",argv[1],"filesink",argv[2], "HD");
	}
	else if(memcmp(argv[1] ,"tcp", 3) == 0)                                                   // for tcp streaming  
	{	
		 set_config(&config, WIDTH_1K, HEIGHT_1K, "mpegtsmux",argv[1],"tcpserversink",argv[2],"TCP_STREAMING");
	}
	else
	{
           exit(0);
	}	
	
		
	GstElement *source, *filter, *h264, *mux, *que, *sink;
	GstCaps *caps;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	
	/* Initialize GStreamer */
	gst_init(&argc, &argv);
	
	/* Create the empty pipeline */
	pipeline = gst_pipeline_new("pipeline");

	source = gst_element_factory_make("qtiqmmfsrc", "source");
	filter = gst_element_factory_make("capsfilter", "filter");
	h264 = gst_element_factory_make("h264parse", "h264");
	mux = gst_element_factory_make(config.mux, "mux");
	que = gst_element_factory_make("queue", "que");
	sink = gst_element_factory_make(config.sink_ops, "sink");
	
	caps = gst_caps_new_simple("video/x-h264", "format", G_TYPE_STRING, "NV12", "width", G_TYPE_INT, config.width, "height", G_TYPE_INT, config.height, "framerate", GST_TYPE_FRACTION, FPS,1,NULL);

	g_object_set (source, "name", "qmmf", NULL);
	g_object_set(filter, "caps", caps, NULL);
	gst_caps_unref(caps);

	if ((memcmp(argv[1] ,"tcp", 3)==0)){
		g_object_set (h264, "config-interval", 1, NULL);	
   		g_object_set (sink, "port", PORT, "host", config.dst, NULL);
	}else
	{
	   g_object_set(sink, "location", config.dst, NULL);
	}

	
	if (!pipeline || !source || !filter || !h264 || !mux || !que || !sink) {
		g_printerr("Not all elements could be created\n");
	}
	
	/* Build the pipeline */
	gst_bin_add_many(GST_BIN(pipeline), source, filter, h264, mux, que, sink, NULL);
	if (!gst_element_link_many(source, filter, h264, mux, que, sink, NULL)) {
		g_printerr("Elements could not be linked\n");
	}
    
     retpid = fork(); 
     if(retpid == 0 )
     {
           g_print("inside the child process\n");

          
          if(memcmp(argv[3] ,"c++", 3) == 0)
		  {
               prog = 0;		
		  } else if(memcmp(argv[3] ,"python", 6) == 0){
             prog = 1;
		 }
          

           if(prog == 0)
			{
            char *programName = "./publish";   
			char *args[] = {programName, "--device", "QCS610", "--recording",config.name,"--action","RECORDING", "--client_id", "basicPubSub", NULL};
            execvp(programName, args);
           
}		   else if(prog == 1)
           {
            char *programName = "python3";   
			char *args[] = {programName, "basicPubSub.py", "-D", "QCS610", "-R",config.name,"-A","IDLE", NULL};
            execvp(programName, args);
           }

     }
     
    waitpid(retpid, &status, 0);
    //Option is 0 since I check it later

    if (WIFSIGNALED(status)){
        g_print("Error\n");
    }
    else if (WEXITSTATUS(status)){
        g_print("Exited Normally\n");
    }
		
	/* Start playing */
	g_print("Setting pipeline to the playing state\n");
	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (ret = GST_STATE_CHANGE_FAILURE) {	
		g_printerr("Unable to set the pipeline to the playing state\n");
	}

	bus = gst_element_get_bus (pipeline);

	signal(SIGINT, sigintHandler);
	
	/* Wait until error or EOS */
	msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* Parse message */
  	if (msg != NULL) {
    		GError *err;
    		gchar *debug_info;

    		switch (GST_MESSAGE_TYPE (msg)) {
      			case GST_MESSAGE_ERROR:
        			gst_message_parse_error (msg, &err, &debug_info);
        			g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
        			g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
        			g_clear_error (&err);
        			g_free (debug_info);
        			break;
    		}
	gst_message_unref(msg);
	}
	
	/* Free resources */
	g_print("Freeing the resources\n");
	gst_object_unref (bus);
	g_print("Setting pipeline to the NULL state\n");
  	gst_element_set_state (pipeline, GST_STATE_NULL);
  	gst_object_unref (pipeline); 
	return 0;	
}


