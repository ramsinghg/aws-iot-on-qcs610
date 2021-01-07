/*
 * Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <aws/crt/Api.h>
#include <aws/crt/StlAllocator.h>

#include <aws/iot/MqttClient.h>

#include <algorithm>
#include <aws/crt/UUID.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include "JSON/JSON.h"



using namespace Aws::Crt;

static void s_printHelp()
{
    fprintf(stdout, "Usage:\n");
    fprintf(
        stdout,
        "publish --device <deviceName> --recording <recordingFormat>"
        " --action <recordingStatus> --topic --ca_file <optional: path to custom ca>"
        " --use_websocket --signing_region <region> --proxy_host <host> --proxy_port <port>\n\n");

    fprintf(stdout, "client_id: client id to use (optional)\n");
    fprintf(
        stdout,
        "ca_file: Optional, if the mqtt server uses a certificate that's not already"
        " in your trust store, set this.\n");
    fprintf(stdout, "\tIt's the path to a CA file in PEM format\n");
    fprintf(stdout, "use_websocket: if specified, uses a websocket over https (optional)\n");
    fprintf(
        stdout,
        "signing_region: used for websocket signer it should only be specific if websockets are used. (required for "
        "websockets)\n");
    fprintf(stdout, "proxy_host: if you want to use a proxy with websockets, specify the host here (optional).\n");
    fprintf(
        stdout, "proxy_port: defaults to 8080 is proxy_host is set. Set this to any value you'd like (optional).\n\n");
}

bool s_cmdOptionExists(char **begin, char **end, const String &option)
{
    return std::find(begin, end, option) != end;
}

char *s_getCmdOption(char **begin, char **end, const String &option)
{
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

int main(int argc, char *argv[])
{

    /************************ Setup the Lib ****************************/
    /*
     * Do the global initialization for the API.
     */
    ApiHandle apiHandle;

    String endpoint = "xxxxxx-ats.iot.us-west-2.amazonaws.com";
    String certificatePath = "./certificates/xxxxxx-certificate.pem.crt";
    String keyPath = "./certificates/xxxxxxxx-private.pem.key";
    String caFile = "./certificates/root-CA.crt";
    String topic = "sdk/test/Python";
    
    String device;
	String status;
	String record_format;
    String clientId(Aws::Crt::UUID().ToString());
    String signingRegion;
    String proxyHost;
    uint16_t proxyPort(8080);

    bool useWebSocket = false;

    /*********************** Parse Arguments ***************************/
    
    


    if (!(s_cmdOptionExists(argv, argv + argc, "--device") && s_cmdOptionExists(argv, argv + argc, "--recording") && s_cmdOptionExists(argv, argv + argc, "--action")))
    {
        s_printHelp();
        return 0;
    }


    if (s_cmdOptionExists(argv, argv + argc, "--device"))
    {
        device = s_getCmdOption(argv, argv + argc, "--device");
    }

    if (s_cmdOptionExists(argv, argv + argc, "--action"))
    {

		status = s_getCmdOption(argv, argv + argc, "--action");
    }

 
   if (s_cmdOptionExists(argv, argv + argc, "--recording"))
    {
        record_format = s_getCmdOption(argv, argv + argc, "--recording");
    }



    if (s_cmdOptionExists(argv, argv + argc, "--client_id"))
    {
        clientId = s_getCmdOption(argv, argv + argc, "--client_id");
    }
    if (s_cmdOptionExists(argv, argv + argc, "--use_websocket"))
    {
        if (!s_cmdOptionExists(argv, argv + argc, "--signing_region"))
        {
            s_printHelp();
        }
        useWebSocket = true;
        signingRegion = s_getCmdOption(argv, argv + argc, "--signing_region");

        if (s_cmdOptionExists(argv, argv + argc, "--proxy_host"))
        {
            proxyHost = s_getCmdOption(argv, argv + argc, "--proxy_host");
        }

        if (s_cmdOptionExists(argv, argv + argc, "--proxy_port"))
        {
            proxyPort = static_cast<uint16_t>(atoi(s_getCmdOption(argv, argv + argc, "--proxy_port")));
        }
    }



    /********************** Now Setup an Mqtt Client ******************/
    /*
     * You need an event loop group to process IO events.
     * If you only have a few connections, 1 thread is ideal
     */  
    Io::EventLoopGroup eventLoopGroup(1);
    if (!eventLoopGroup)
    {
        fprintf(
            stderr, "Event Loop Group Creation failed with error %s\n", ErrorDebugString(eventLoopGroup.LastError()));
        exit(-1);
    }

    Aws::Crt::Io::DefaultHostResolver defaultHostResolver(eventLoopGroup, 1, 5);
    Io::ClientBootstrap bootstrap(eventLoopGroup, defaultHostResolver);

    if (!bootstrap)
    {
        fprintf(stderr, "ClientBootstrap failed with error %s\n", ErrorDebugString(bootstrap.LastError()));
        exit(-1);
    }

    Aws::Iot::MqttClientConnectionConfigBuilder builder;

    if (!certificatePath.empty() && !keyPath.empty())
    {
        builder = Aws::Iot::MqttClientConnectionConfigBuilder(certificatePath.c_str(), keyPath.c_str());
    }
    else if (useWebSocket)
    {
        Aws::Iot::WebsocketConfig config(signingRegion, &bootstrap);

        if (!proxyHost.empty())
        {
            Aws::Crt::Http::HttpClientConnectionProxyOptions proxyOptions;
            proxyOptions.HostName = proxyHost;
            proxyOptions.Port = proxyPort;
            proxyOptions.AuthType = Aws::Crt::Http::AwsHttpProxyAuthenticationType::None;
            config.ProxyOptions = std::move(proxyOptions);
        }

        builder = Aws::Iot::MqttClientConnectionConfigBuilder(config);
    }
    else
    {
        s_printHelp();
    }

    if (!caFile.empty())
    {
        builder.WithCertificateAuthority(caFile.c_str());
    }

    builder.WithEndpoint(endpoint);

    auto clientConfig = builder.Build();

    if (!clientConfig)
    {
        fprintf(
            stderr,
            "Client Configuration initialization failed with error %s\n",
            ErrorDebugString(clientConfig.LastError()));
        exit(-1);
    }

    Aws::Iot::MqttClient mqttClient(bootstrap);
    /*
     * Since no exceptions are used, always check the bool operator
     * when an error could have occurred.
     */
    if (!mqttClient)
    {
        fprintf(stderr, "MQTT Client Creation failed with error %s\n", ErrorDebugString(mqttClient.LastError()));
        exit(-1);
    }

    /*
     * Now create a connection object. Note: This type is move only
     * and its underlying memory is managed by the client.
     */
    auto connection = mqttClient.NewConnection(clientConfig);

    if (!connection)
    {
        fprintf(stderr, "MQTT Connection Creation failed with error %s\n", ErrorDebugString(mqttClient.LastError()));
        exit(-1);
    }

    /*
     * In a real world application you probably don't want to enforce synchronous behavior
     * but this is a sample console application, so we'll just do that with a condition variable.
     */
    std::mutex mutex;
    std::condition_variable conditionVariable;
    bool connectionSucceeded = false;
    bool connectionClosed = false;
    bool connectionCompleted = false;

    /*
     * This will execute when an mqtt connect has completed or failed.
     */
    auto onConnectionCompleted = [&](Mqtt::MqttConnection &, int errorCode, Mqtt::ReturnCode returnCode, bool) {
        if (errorCode)
        {
            fprintf(stdout, "Connection failed with error %s\n", ErrorDebugString(errorCode));
            std::lock_guard<std::mutex> lockGuard(mutex);
            connectionSucceeded = false;
        }
        else
        {
            fprintf(stdout, "Connection completed with return code %d\n", returnCode);
            connectionSucceeded = true;
        }
        {
            std::lock_guard<std::mutex> lockGuard(mutex);
            connectionCompleted = true;
        }
        conditionVariable.notify_one();
    };

    auto onInterrupted = [&](Mqtt::MqttConnection &, int error) {
        fprintf(stdout, "Connection interrupted with error %s\n", ErrorDebugString(error));
    };

    auto onResumed = [&](Mqtt::MqttConnection &, Mqtt::ReturnCode, bool) { fprintf(stdout, "Connection resumed\n"); };

    /*
     * Invoked when a disconnect message has completed.
     */
    auto onDisconnect = [&](Mqtt::MqttConnection &) {
        {
            fprintf(stdout, "Disconnect completed\n");
            std::lock_guard<std::mutex> lockGuard(mutex);
            connectionClosed = true;
        }
        conditionVariable.notify_one();
    };

    connection->OnConnectionCompleted = std::move(onConnectionCompleted);
    connection->OnDisconnect = std::move(onDisconnect);
    connection->OnConnectionInterrupted = std::move(onInterrupted);
    connection->OnConnectionResumed = std::move(onResumed);

    connection->SetOnMessageHandler([](Mqtt::MqttConnection &, const String &topic, const ByteBuf &payload) {
        fprintf(stdout, "Generic Publish received on topic %s, payload:\n", topic.c_str());
        fwrite(payload.buffer, 1, payload.len, stdout);
        fprintf(stdout, "\n");
    });

    /*
     * Actually perform the connect dance.
     * This will use default ping behavior of 1 hour and 3 second timeouts.
     * If you want different behavior, those arguments go into slots 3 & 4.
     */
    fprintf(stdout, "Connecting...\n");
    if (!connection->Connect(clientId.c_str(), false, 1000))
    {
        fprintf(stderr, "MQTT Connection failed with error %s\n", ErrorDebugString(connection->LastError()));
        exit(-1);
    }

    std::unique_lock<std::mutex> uniqueLock(mutex);
    conditionVariable.wait(uniqueLock, [&]() { return connectionCompleted; });

    if (connectionSucceeded)
    {
        /*
         * This is invoked upon the receipt of a Publish on a subscribed topic.
         */
        auto onPublish = [&](Mqtt::MqttConnection &, const String &topic, const ByteBuf &byteBuf) {
            fprintf(stdout, "Publish received on topic %s\n", topic.c_str());
            fprintf(stdout, "\n Message:\n");
            fwrite(byteBuf.buffer, 1, byteBuf.len, stdout);
            fprintf(stdout, "\n");
        };

        /*
         * Subscribe for incoming publish messages on topic.
         */
        auto onSubAck = [&](Mqtt::MqttConnection &, uint16_t packetId, const String &topic, Mqtt::QOS, int errorCode) {
            if (packetId)
            {
                fprintf(stdout, "Subscribe on topic %s on packetId %d Succeeded\n", topic.c_str(), packetId);
            }
            else
            {
                fprintf(stdout, "Subscribe failed with error %s\n", aws_error_debug_str(errorCode));
            }
            conditionVariable.notify_one();
        };

        connection->Subscribe(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, onPublish, onSubAck);
        conditionVariable.wait(uniqueLock);

        if (true)
        {


		cJSON *root;
		root=cJSON_CreateObject();
        cJSON_AddStringToObject(root, "Device", device.c_str());
        cJSON_AddStringToObject(root, "Recording",record_format.c_str());
        cJSON_AddStringToObject(root, "Action", status.c_str());
	
	    char* str = cJSON_Print(root);  

           
            ByteBuf payload = ByteBufNewCopy(DefaultAllocator(), (const uint8_t *) str, strlen(str));
            ByteBuf *payloadPtr = &payload;

            auto onPublishComplete = [payloadPtr](Mqtt::MqttConnection &, uint16_t packetId, int errorCode) {
                aws_byte_buf_clean_up(payloadPtr);

                if (packetId)
                {
                    fprintf(stdout, "Operation on packetId %d Succeeded\n", packetId);
                }
                else
                {
                    fprintf(stdout, "Operation failed with error %s\n", aws_error_debug_str(errorCode));
                }
            };
            connection->Publish(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload, onPublishComplete);
        }

        /*
         * Unsubscribe from the topic.
         */
        connection->Unsubscribe(
            topic.c_str(), [&](Mqtt::MqttConnection &, uint16_t, int) { conditionVariable.notify_one(); });
        conditionVariable.wait(uniqueLock);
    }

    /* Disconnect */
    if (connection->Disconnect())
    {
        conditionVariable.wait(uniqueLock, [&]() { return connectionClosed; });
    }
    return 0;
}
