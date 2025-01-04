/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#include <iostream>
#include <memory>

#include <xrapp.hpp>

using namespace xrlib;
using namespace xrapp;

namespace app
{
	class App : public XrApp
	{
	  public:

		#ifdef XR_USE_PLATFORM_ANDROID

			App( 
				struct android_app *pAndroidApp, 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );

			void Init();
		#else
			App( 
				int argc, 
				char *argv[], 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );

			int Init();
		#endif

			~App();

			void SetupScene();
			void ProcessXrEvents( XrEventDataBaseHeader &xrEventDataBaseheader );

			bool StartRenderFrame();
			void EndRenderFrame();

			struct SPipelines
			{
				uint16_t primitiveLayout = 0;
				uint32_t primitives = 0;
			}pipelines;

	  private:
			void CreateGraphicsPipelines();

	};

} // namespace xrapp