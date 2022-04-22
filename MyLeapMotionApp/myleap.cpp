#include<stdio.h>
#include<stdlib.h>

#include "LeapC.h"
#include "MyLeapMotionApp.h"

#if defined(_MSC_VER)
#include<Windows.h>
#include<process.h>
#define LockMutex EnterCriticalSection
#define UnlockMutex LeaveCriticalSection
#endif

#if defined(_MSC_VER)
static HANDLE pollingThread;
static CRITICAL_SECTION dataLock;
#endif

static volatile bool _isRunning = false;
static LEAP_CONNECTION connectionHandle = NULL;
static LEAP_TRACKING_EVENT* lastFrame = NULL;

typedef void (*tracking_callback)       (const LEAP_TRACKING_EVENT* tracking_event);

void CloseConnection();
void DestroyConnection();

struct Callbacks {
    //connection_callback      on_connection;
    //connection_callback      on_connection_lost;
    //device_callback          on_device_found;
    //device_lost_callback     on_device_lost;
    //device_failure_callback  on_device_failure;
    //policy_callback          on_policy;
    tracking_callback        on_frame;
    //log_callback             on_log_message;
    //config_change_callback   on_config_change;
    //config_response_callback on_config_response;
    //image_callback           on_image;
    //point_mapping_change_callback on_point_mapping_change;
    //head_pose_callback       on_head_pose;
};

struct Callbacks ConnectionCallbacks;

const char* ResultString(eLeapRS r) {
    switch (r) {
    case eLeapRS_Success:                  return "eLeapRS_Success";
    case eLeapRS_UnknownError:             return "eLeapRS_UnknownError";
    case eLeapRS_InvalidArgument:          return "eLeapRS_InvalidArgument";
    case eLeapRS_InsufficientResources:    return "eLeapRS_InsufficientResources";
    case eLeapRS_InsufficientBuffer:       return "eLeapRS_InsufficientBuffer";
    case eLeapRS_Timeout:                  return "eLeapRS_Timeout";
    case eLeapRS_NotConnected:             return "eLeapRS_NotConnected";
    case eLeapRS_HandshakeIncomplete:      return "eLeapRS_HandshakeIncomplete";
    case eLeapRS_BufferSizeOverflow:       return "eLeapRS_BufferSizeOverflow";
    case eLeapRS_ProtocolError:            return "eLeapRS_ProtocolError";
    case eLeapRS_InvalidClientID:          return "eLeapRS_InvalidClientID";
    case eLeapRS_UnexpectedClosed:         return "eLeapRS_UnexpectedClosed";
    case eLeapRS_UnknownImageFrameRequest: return "eLeapRS_UnknownImageFrameRequest";
    case eLeapRS_UnknownTrackingFrameID:   return "eLeapRS_UnknownTrackingFrameID";
    case eLeapRS_RoutineIsNotSeer:         return "eLeapRS_RoutineIsNotSeer";
    case eLeapRS_TimestampTooEarly:        return "eLeapRS_TimestampTooEarly";
    case eLeapRS_ConcurrentPoll:           return "eLeapRS_ConcurrentPoll";
    case eLeapRS_NotAvailable:             return "eLeapRS_NotAvailable";
    case eLeapRS_NotStreaming:             return "eLeapRS_NotStreaming";
    case eLeapRS_CannotOpenDevice:         return "eLeapRS_CannotOpenDevice";
    default:                               return "unknown result type.";
    }
}

void setFrame(const LEAP_TRACKING_EVENT* frame) {
    LockMutex(&dataLock);
    if (!lastFrame) lastFrame = (LEAP_TRACKING_EVENT*)malloc(sizeof(*frame));
    *lastFrame = *frame;
    UnlockMutex(&dataLock);
}

static void handleTrackingEvent(const LEAP_TRACKING_EVENT* tracking_event) {
    setFrame(tracking_event); //support polling tracking data from different thread
    if (ConnectionCallbacks.on_frame) {
        ConnectionCallbacks.on_frame(tracking_event);
    }
}

#if defined(_MSC_VER)
static void serviceMessageLoop(void* unused) {
#endif
    eLeapRS result;
    LEAP_CONNECTION_MESSAGE msg;

    while (_isRunning) {
        unsigned int timeout = 1000;
        result = LeapPollConnection(connectionHandle, timeout, &msg);

        if (result != eLeapRS_Success) {
            printf("LeapC PollConnection call was %s.\n", ResultString(result));
            continue;
        }

        switch (msg.type) {
        case eLeapEventType_Connection:
            //handleConnectionEvent(msg.connection_event);
            break;
        case eLeapEventType_ConnectionLost:
            //handleConnectionLostEvent(msg.connection_lost_event);
            break;
        case eLeapEventType_Device:
            //handleDeviceEvent(msg.device_event);
            break;
        case eLeapEventType_DeviceLost:
            //handleDeviceLostEvent(msg.device_event);
            break;
        case eLeapEventType_DeviceFailure:
            //handleDeviceFailureEvent(msg.device_failure_event);
            break;
        case eLeapEventType_Tracking:
            handleTrackingEvent(msg.tracking_event);
            break;
        case eLeapEventType_ImageComplete:
            // Ignore
            break;
        case eLeapEventType_ImageRequestError:
            // Ignore
            break;
        case eLeapEventType_LogEvent:
            //handleLogEvent(msg.log_event);
            break;
        case eLeapEventType_Policy:
            //(msg.policy_event);
            break;
        case eLeapEventType_ConfigChange:
            //handleConfigChangeEvent(msg.config_change_event);
            break;
        case eLeapEventType_ConfigResponse:
            //handleConfigResponseEvent(msg.config_response_event);
            break;
        case eLeapEventType_Image:
            //handleImageEvent(msg.image_event);
            break;
        case eLeapEventType_PointMappingChange:
            //handlePointMappingChangeEvent(msg.point_mapping_change_event);
            break;
        case eLeapEventType_LogEvents:
            //handleLogEvents(msg.log_events);
            break;
        case eLeapEventType_HeadPose:
            //handleHeadPoseEvent(msg.head_pose_event);
            break;
        default:
            //discard unknown message types
            printf("Unhandled message type %i.\n", msg.type);
        } //switch on msg.type
    }
#if !defined(_MSC_VER)
    return NULL;
#endif
}

LEAP_CONNECTION OpenConnection() {//LEAP_CONNECTION* OpenConnection() {
    if (_isRunning) {
        return connectionHandle;//return &connectionHandle;
    }
    if (connectionHandle || LeapCreateConnection(NULL, &connectionHandle) == eLeapRS_Success) {
        eLeapRS result = LeapOpenConnection(connectionHandle);
        if (result == eLeapRS_Success) {
            _isRunning = true;
#if defined(_MSC_VER)
            InitializeCriticalSection(&dataLock);
            pollingThread = (HANDLE)_beginthread(serviceMessageLoop, 0, NULL);
#endif
        }
    }
    return connectionHandle;//return &connectionHandle;
}

static void* allocate(uint32_t size, eLeapAllocatorType typeHint, void* state) {
    void* ptr = malloc(size);
    return ptr;
}

static void deallocate(void* ptr, void* state) {
    if (!ptr)
        return;
    free(ptr);
}

static void OnFrame(const LEAP_TRACKING_EVENT* frame) {
    if (frame->info.frame_id % 60 == 0)
        printf("Frame %lli with %i hands.\n", (long long int)frame->info.frame_id, frame->nHands);

    for (uint32_t h = 0; h < frame->nHands; h++) {
        LEAP_HAND* hand = &frame->pHands[h];
        printf("    Hand id %i is a %s hand with position (%f, %f, %f).\n",
            hand->id,
            (hand->type == eLeapHandType_Left ? "left" : "right"),
            hand->palm.position.x,
            hand->palm.position.y,
            hand->palm.position.z);

        if (h < 2) {
            hands[h].left = (hand->type == eLeapHandType_Left);
            hands[h].x = hand->palm.position.x;
            hands[h].y = hand->palm.position.y;
            hands[h].z = hand->palm.position.z;
        }
    }
}

void Startleap() {
    ConnectionCallbacks.on_frame = &OnFrame;

    connectionHandle = OpenConnection();
    {
        LEAP_ALLOCATOR allocator = { allocate, deallocate, NULL };
        LeapSetAllocator(connectionHandle, &allocator);//LeapSetAllocator(*connectionHandle, &allocator);
    }
    LeapSetPolicyFlags(connectionHandle, eLeapPolicyFlag_Images | eLeapPolicyFlag_MapPoints, 0);//LeapSetPolicyFlags(*connectionHandle, eLeapPolicyFlag_Images | eLeapPolicyFlag_MapPoints, 0);

    printf("Press Enter to exit program.\n");
    getchar();
}

void Closeleap() {
    CloseConnection();
    DestroyConnection();
}

struct Handpositions getposition(int i) {
    if (i < 2) {
        return hands[i];
    }
    return hands[0];
}

void CloseConnection() {
    if (!_isRunning) {
        return;
    }
    _isRunning = false;
    LeapCloseConnection(connectionHandle);
#if defined(_MSC_VER)
    WaitForSingleObject(pollingThread, INFINITE);
    //CloseHandle(pollingThread);
#endif
}

void DestroyConnection() {
    CloseConnection();
    LeapDestroyConnection(connectionHandle);
}