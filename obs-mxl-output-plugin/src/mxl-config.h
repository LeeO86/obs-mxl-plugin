/*
-------------------------------------------------------------------------------------------
  Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

  Licensed under the Apache License, Version 2.0 (the "License").
  You may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-------------------------------------------------------------------------------------------
*/

#ifndef MXL_CONFIG_H
#define MXL_CONFIG_H

#include <string>
#include <obs-module.h>

#define MXL_SECTION_NAME "MXLPlugin"
#define MXL_PARAM_OUTPUT_ENABLED "OutputEnabled"
#define MXL_PARAM_DOMAIN_PATH "DomainPath"
#define MXL_PARAM_VIDEO_ENABLED "VideoEnabled"
#define MXL_PARAM_VIDEO_FLOW_ID "VideoFlowId"
#define MXL_PARAM_AUDIO_ENABLED "AudioEnabled"
#define MXL_PARAM_AUDIO_FLOW_ID "AudioFlowId"

class MXLConfig {
public:
    MXLConfig();
    static MXLConfig* Current();
    void Load();
    void Save();
    std::string GetConfigPath();

    bool OutputEnabled;
    std::string DomainPath;
    bool VideoEnabled;
    std::string VideoFlowId;
    bool AudioEnabled;
    std::string AudioFlowId;

private:
    static MXLConfig* _instance;
};

#endif // MXL_CONFIG_H
