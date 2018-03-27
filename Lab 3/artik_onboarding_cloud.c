

// Copy these lines after CloudIsSecureDeviceType function

static pthread_addr_t start_streaming(pthread_addr_t arg) {
	int num_posts=100; // number of times sending the sensor data
	int i=0;
	struct timespec timestamp;

	printf("Start streaming sensor data\n");
	for(i=0; i<num_posts;i++) {
		int val = ReadSensor(0);
		char message[100] = "";

		clock_gettime(CLOCK_REALTIME, &timestamp);
		snprintf(message, 100, "{\"sensor\":%d, \"timestamp\":%ld}", val, timestamp.tv_sec);
		printf(message);

		SendMessageToCloud(message);
		usleep(10000 * 1000);
	}
}

artik_error StartStreaming() {
	artik_error ret = S_OK;
	static pthread_t tid;
	pthread_attr_t attr;
	int status;


	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_setstacksize(&attr, 16 * 1024);

	status = pthread_create(&tid, &attr, start_streaming, NULL);
	if (status) {
		printf("Failed to create thread for sensor streaming\n");
		ret = E_NO_MEM;
		goto exit;
	}
	pthread_setname_np(tid, "streaming start");
	pthread_join(tid, NULL);
	printf("Sensor Streaming successfully connected\n");

	exit:     return ret;
}

