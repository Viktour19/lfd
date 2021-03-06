//controller.cpp

#include "servoController/controllerInterface.h"
#include "lfdApplicaton/appImplementation.h"

struct timespec counter, start, start2, pressKey, releaseKey;

int main()
{
    FILE *fp_in;
    if ((fp_in = fopen("applicationControl/objectTrackingInput.txt", "r")) == 0)
    {
        printf("Error can't open input file objectTrackingInput.txt\n");
        prompt_and_exit(1);
    }

    int *segmentation_values = new int[4];
    int min_hue;
    int max_hue;
    int min_sat;
    int max_sat;

    fscanf(fp_in, "%d %d %d %d", &min_hue, &max_hue, &min_sat, &max_sat);

    segmentation_values[0] = min_hue;
    segmentation_values[1] = max_hue;
    segmentation_values[2] = min_sat;
    segmentation_values[3] = max_sat;

    FILE *fp_in2;
    if ((fp_in2 = fopen("applicationControl/controlInput.txt", "r")) == 0)
    {
        printf("Error can't open input file controlInput.txt\n");
        prompt_and_exit(1);
    }

    // Camera Index
    int idx = CAM_IDX1;
    int idx2 = CAM_IDX2;

    // Camera Capture
    cv::VideoCapture cap;
    cv::Mat frame;

    // Camera Capture
    cv::VideoCapture cap2;
    cv::Mat frame2;

    if (!cap.open(idx2))
    {
        cout << "Webcam not connected.\n"
             << "Please verify\n";

        prompt_and_exit(1);
    }

    if (!cap2.open(idx))
    {
        cout << "Webcam not connected.\n"
             << "Please verify\n";

        prompt_and_exit(1);
    }

    cap.set(CV_CAP_PROP_FRAME_WIDTH, 1024);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 768);

    cap2.set(CV_CAP_PROP_FRAME_WIDTH, 1024);
    cap2.set(CV_CAP_PROP_FRAME_HEIGHT, 768);

    char PORT[13] = "/dev/ttyUSB0"; // USB Port ID on windows - "\\\\.\\COM9";
    char BAUD[7] = "9600";          // Baud Rate
    int speed = 300;                // Servo speed

    float x = 0;
    float y = 120;
    float z = 200;
    float pitch = -180;
    float roll = -90;
    int graspVal = 0; //open

    initializeControllerWithSpeed(PORT, BAUD, speed);

    goHome(5);

    gotoPose(x, y, z, pitch, roll);

#if DEMO

    printf("\n %s \n", "Commence demonstration");

    FILE *training_file = fopen("applicationData/trainingdata.txt", "a");
    FILE *training_file_2 = fopen("applicationData/trainingdata2.txt", "a");

    if (training_file == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    spnav_event sev;
    signal(SIGINT, sig);

    // spnav_sensitivity(0.1f);

    if (spnav_open() == -1)
    {
        fprintf(stderr, "failed to connect to the space navigator daemon\n");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    bool _continuedemo = true;

    while (_continuedemo)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &counter);

        float *poseDelta = new float[4]; //delta (x, y, z, theta)

        poseDelta[0] = 0.0;
        poseDelta[1] = 0.0;
        poseDelta[2] = 0.0;
        poseDelta[3] = 0.0;
        poseDelta[4] = 0.0;

        if (timediff(counter, start) >= 500) //sample every 200 milliseconds or 5Hz
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);

            float *objectpose = new float[4]; //delta (x, y, z, theta)

            objectpose[0] = -1.0;
            objectpose[1] = -1.0;
            objectpose[2] = -1.0;
            objectpose[3] = -1.0;

            // Observe
            // Observation: objectpose - endeffector pose

            cap >> frame;
            //camera 1 (down ward facing)
            float *ff = getObjectPose(frame, segmentation_values);

            cap2 >> frame2;
            //camera 2 (side ward facing)
            float *ff2 = getObjectPose(frame2, segmentation_values);

            if (ff[0] != -1)
            {
                if (ff[3] != -1)
                {
                    objectpose[0] = (ff[3] - ff[0]); //x
                    objectpose[1] = (ff[4] - ff[1]); //y
                    objectpose[3] = (ff[5] - ff[2]); //angle

                    if (ff2[1] != -1)
                    {
                        if (ff2[4] != -1)
                        {
                            objectpose[2] = +(ff2[4] - ff2[1]); //z
                        }
                    }
                }
            }

            //endeffector and object differential pose

            objectpose[0] += 0.5;
            objectpose[1] += 0.5;
            objectpose[2] += 0.5;
            objectpose[3] += 0.5;

            fprintf(training_file, " %d %d %d %d %d\n", (int)poseDelta[0], (int)poseDelta[1], (int)poseDelta[2], (int)poseDelta[4], (int)graspVal);
            fprintf(training_file_2, " %d %d %d %d %d\n", (int)(poseDelta[0]), (int)(poseDelta[1]), (int)(poseDelta[2]), (int)(poseDelta[4]), (int)graspVal);

            fprintf(training_file, " %d %d %d %d %d\n", (int)objectpose[0], (int)objectpose[1], (int)objectpose[2], (int)objectpose[3], (int)graspVal);
            fprintf(training_file_2, " %d %d %d %d %d\n", (int)(objectpose[0] / 2.0), (int)(objectpose[1] / 2.0), (int)(objectpose[2] / 2.0), (int)(objectpose[3] / 2.0), (int)graspVal);
        }

        if (timediff(counter, start2) >= 500) //sample every 200 milliseconds or 5Hz
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start2);

            int type = spnav_poll_event(&sev);
            if (type == SPNAV_EVENT_MOTION)
            {
                poseDelta = scale_and_map(sev.motion.x, sev.motion.y, sev.motion.z, sev.motion.rx, sev.motion.ry, sev.motion.rz);

                //take the max delta only and act on only that axis

                int index = -1;

                if(fabs(poseDelta[0]) >= fabs(poseDelta[1]))
                    index = 0;
                if (fabs(poseDelta[0]) < fabs(poseDelta[1]))
                    index = 1;
                if (fabs(poseDelta[index]) < fabs(poseDelta[2]))
                    index = 2;
                if (fabs(poseDelta[index]) < fabs(poseDelta[3]))
                    index = 3;
                if (fabs(poseDelta[index]) < fabs(poseDelta[4]))
                    index = 4;

                int status = 0;
                printf("\n index : %d and %d \n", index,  poseDelta[index]);

                if (index == 0 && poseDelta[index] != 0.0)
                    status = gotoPose(x + (poseDelta[0] < 0.0 ? -3 : 3), y, z, pitch, roll);

                else if (index == 1)
                    status = gotoPose(x, y + (poseDelta[1] < 0.0 ? -3 : 3), z, pitch, roll);

                else if (index == 2)
                    status = gotoPose(x, y, z + (poseDelta[2] < 0.0 ? -3 : 3), pitch, roll);

                else if (index == 4)
                    status = gotoPose(x, y, z, pitch, roll);
                // status = gotoPose(x, y, z, pitch, roll + (poseDelta[4] < 0.0 ? -3 : 3));

                if (status)
                {
                    //Action: posedeltas

                    if (index == 0)
                    {
                        poseDelta[0] = (poseDelta[0] < 0.0) ? -3.0 : 3.0;
                        x += poseDelta[0];
                    }
                    else if (index == 1)
                    {
                        poseDelta[1] = (poseDelta[1] < 0.0) ? -3.0 : 3.0;
                        y += poseDelta[1];
                    }
                    else if (index == 2)
                    {
                        poseDelta[2] = (poseDelta[2] < 0.0) ? -3.0 : 3.0;
                        z += poseDelta[2];
                    }
                    else if (index == 4)
                    {
                        poseDelta[4] = (poseDelta[4] < 0.0) ? -3.0 : 3.0;
                        // roll += poseDelta[4];
                    }
                }

                poseDelta[0] = 0.0;
                poseDelta[1] = 0.0;
                poseDelta[2] = 0.0;
                poseDelta[3] = 0.0;
            }

            if (type == SPNAV_EVENT_BUTTON)
            { /* SPNAV_EVENT_BUTTON */

                if (sev.button.bnum == 0 && sev.button.press)
                {
                    graspVal = abs(graspVal - 30);

                    printf("%d\n", graspVal);
                    grasp(graspVal);
                }

                else if (sev.button.bnum == 1)
                {
                    if (sev.button.press)
                        clock_gettime(CLOCK_MONOTONIC_RAW, &pressKey);
                    else
                        clock_gettime(CLOCK_MONOTONIC_RAW, &releaseKey);

                    //if it took at least 3 seconds to release, then end the demo
                    if (timediff(releaseKey, pressKey) > 3000 && releaseKey.tv_sec != 0)
                    {

                        printf("\n %s \n", "End of Demonstration");

                        gotoPose(0, 120, 200, pitch, -90);

                        graspVal = 0;
                        grasp(graspVal);

                        fclose(training_file);
                        _continuedemo = false;
                    }
                    else
                    {
                        if (!sev.button.press)
                        {
                            gotoPose(0, 120, 200, pitch, -90);

                            fprintf(training_file, "%s\n", "finish");
                            fprintf(training_file_2, "%s\n", "finish");
                            printf("\n %s \n", "End of current Demonstration");

                            graspVal = 0;
                            grasp(graspVal);

                            x = 0;
                            y = 120;
                            z = 200;
                            roll = -90;

                            sleep(2);
                        }
                    }
                }
            }

            spnav_remove_events(SPNAV_EVENT_ANY);
        }
    }

#endif

#if TRAIN

    FILE *execution_file = fopen("applicationData/executiondata.txt", "w");
    FILE *hypotheses_file = fopen("applicationData/hypotheses.txt", "w");

    if (execution_file == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

#if !DEMO

    spnav_event sev;
    signal(SIGINT, sig);

    if (spnav_open() == -1)
    {
        fprintf(stderr, "failed to connect to the space navigator daemon\n");
        return 1;
    }

#endif

    bool trained = false;
    //train PSL using demonstration data

    while (spnav_wait_event(&sev))
    {
        if (sev.type == SPNAV_EVENT_BUTTON)
        {
            if (sev.button.bnum == 1)
            {
                if (sev.button.press)
                    clock_gettime(CLOCK_MONOTONIC_RAW, &pressKey);
                else
                    clock_gettime(CLOCK_MONOTONIC_RAW, &releaseKey);

                if (timediff(releaseKey, pressKey) > 2000)
                {
                    printf("\n %s \n", "Start training");

                    hyptrain();

                    printf("\n %s \n", "Training done");

                    trained = true;

                    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

                    break;
                }
            }
        }
    }

    spnav_close();

    // Event_t *events_ = new Event_t();
    EventUnion e;

    print_hypotheses(hypotheses_file);

    while (true && trained)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &counter);

        if (timediff(counter, start) > 1000) //observe every 1000ms
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);

            // Observation: objectpose - endeffector pose

            cap >> frame;
            float *ff = getObjectPose(frame, segmentation_values);

            cap2 >> frame2;
            float *ff2 = getObjectPose(frame2, segmentation_values);
            float *objectpose = new float[4]; //delta (x, y, z, theta)
            objectpose[0] = -1.0;
            objectpose[1] = -1.0;
            objectpose[2] = -1.0;
            objectpose[3] = -1.0;

            if (ff[0] != -1)
            {
                if (ff[3] != -1)
                {
                    objectpose[0] = (ff[3] - ff[0]);
                    objectpose[1] = (ff[4] - ff[1]);
                    objectpose[3] = (ff[5] - ff[2]);

                    if (ff2[4] != -1)
                    {
                        if (ff2[1] != -1)
                        {
                            objectpose[2] = +(ff2[4] - ff[1]);
                        }
                    }
                }
            }

            // objectpose[0] = -0.388363;
            // objectpose[1] = -1.843223;
            // objectpose[2] = -2.740281;
            // objectpose[3] = -0.007023;

            e.observation.diffX = objectpose[0] + 0.5;
            e.observation.diffY = objectpose[1] + 0.5;
            e.observation.diffZ = objectpose[2] + 0.5;
            e.observation.diffangle = 0.0; //objectpose[3] + 0.5;

            e.observation.grasp = graspVal;

            fprintf(execution_file, "Observation: %f %f %f %f %d\n", objectpose[0], objectpose[1], objectpose[2], objectpose[3], graspVal);
            fprintf(execution_file, "Observation: %d %d %d %d %d\n", (int)e.observation.diffX, (int)e.observation.diffY, (int)e.observation.diffZ, (int)e.observation.diffangle, (int)e.observation.grasp);

            Event_t *events_ = new Event_t();
            push(events_, e, 2);

            Event_t *pred = predict(events_);
            push(events_, pred->event, pred->eventtype);

            // fprintf(execution_file, "Action: %f %f %f %f %f %d\n", (float) pred->event.action.deltaX, (float) pred->event.action.deltaY, (float) pred->event.action.deltaZ, pitch, (float) pred->event.action.deltaangle, pred->event.action.grasp);

            fprintf(execution_file, "Action: %d %d %d %d %d %d evtype: %d \n", (int)pred->event.action.deltaX, (int)pred->event.action.deltaY, (int)pred->event.action.deltaZ, (int)pitch, (int)pred->event.action.deltaangle, (int)pred->event.action.grasp, (int)pred->eventtype);

            if (pred->eventtype != 0)
            {
                // int status = gotoPose(x + (float) pred->event.action.deltaX, y + (float) pred->event.action.deltaY, z + (float) pred->event.action.deltaZ, pitch, roll + (float) pred->event.action.deltaangle);
                int status = gotoPose(x + (float)pred->event.action.deltaX, y + (float)pred->event.action.deltaY, z + (float)pred->event.action.deltaZ, pitch, roll);

                if (status)
                {

                    grasp(pred->event.action.grasp);
                    graspVal = pred->event.action.grasp;

                    x += (float)pred->event.action.deltaX;
                    y += (float)pred->event.action.deltaY;
                    z += (float)pred->event.action.deltaZ;
                    roll += 0.0; //(float) pred->event.action.deltaangle;
                }
            }
        }
    }

    free_hyp();
    fclose(execution_file);

#endif

    spnav_close();
    cap.release();

    //routine for manual input

#if DEBUG

    printf("\nEnter the cartesian pose values and press enter twice...\n");
    printf("\nHit 'q' to exit...\n");

    char buffer = 0;

    x = 100;
    y = 200;
    z = 200;
    pitch = -180;
    roll = 0;

    grasp(0);

    while (buffer != 'q')
    {
        gotoPose(x, y, z, pitch, roll);
        int i = 0;
        float arr[10];
        char temp;

        do
        {
            scanf("%f%c", &arr[i], &temp);
            i++;

        } while (temp != '\n');

        for (int j = 0; j < i; j++)
        {
            switch (j)
            {
            case 0:
                x = arr[j];
                break;

            case 1:
                y = arr[j];
                break;

            case 2:
                z = arr[j];
                break;

            case 3:
                pitch = arr[j];
                break;

            case 4:
                roll = arr[j];
                break;
            };
        }

        buffer = getchar();
    }

#endif

#if CALIBRATE

    sleep(10);

    int min_z = 100;
    int max_z = 270;
    int min_y = 130;
    int max_y = 240;
    int min_x = -90;
    int max_x = 75;
    FILE *f = fopen("applicationData/calibrationdata.txt", "w");

    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    grasp(30);

    for (int x = min_x; x < max_x; x += 20)
    {
        for (int y = min_y; y < max_y; y += 20)
        {
            gotoPose(x, y, min_z, pitch, roll);

            sleep(2);

            cap >> frame;
            float *ff = getObjectPose(frame, segmentation_values);

            printf("\n %f %f %f \n", ff[0], ff[1], ff[2]);

            float i = ff[0];
            float j = ff[1];

            fprintf(f, "%d %d %d %f %f\n", x, y, min_z, i, j);

            sleep(2);
        }
    }

    fclose(f);

#endif

    delete[] segmentation_values;
    return 0;
}
