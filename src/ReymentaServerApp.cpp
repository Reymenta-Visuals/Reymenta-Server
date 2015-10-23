#include "ReymentaServerApp.h"

void ReymentaServerApp::prepare(Settings *settings)
{
	settings->setWindowSize(1440, 900);
}

void ReymentaServerApp::setup()
{
	int wr;
	// parameters
	mParameterBag = ParameterBag::create();
	// utils
	mBatchass = Batchass::create(mParameterBag);
	CI_LOG_V("batchass setup");

	wr = mBatchass->getWindowsResolution();

	setWindowSize(mParameterBag->mMainWindowWidth, mParameterBag->mMainWindowHeight);
	// 12 fps is enough for a router
	setFrameRate(120.0f);
	setWindowPos(ivec2(0, 0));

	// if mStandalone, put on the 2nd screen
	if (mParameterBag->mStandalone)
	{
		setWindowSize(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);
		setWindowPos(ivec2(mParameterBag->mRenderX, mParameterBag->mRenderY));
	}

	// setup shaders and textures
	mBatchass->setup();

	mParameterBag->mMode = MODE_WARP;
	mParameterBag->iResolution.x = mParameterBag->mRenderWidth;
	mParameterBag->iResolution.y = mParameterBag->mRenderHeight;
	mParameterBag->mRenderResolution = ivec2(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);

	CI_LOG_V("createRenderWindow, resolution:" + toString(mParameterBag->iResolution.x) + "x" + toString(mParameterBag->iResolution.y));
	mParameterBag->mRenderResoXY = vec2(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);
	mParameterBag->mRenderPosXY = ivec2(mParameterBag->mRenderX, mParameterBag->mRenderY);

	// instanciate the console class
	mConsole = AppConsole::create(mParameterBag, mBatchass);

	// imgui
	margin = 3;
	inBetween = 3;
	// mPreviewFboWidth 80 mPreviewFboHeight 60 margin 10 inBetween 15 mPreviewWidth = 160;mPreviewHeight = 120;
	w = mParameterBag->mPreviewFboWidth + margin;
	h = mParameterBag->mPreviewFboHeight * 2.3;
	largeW = (mParameterBag->mPreviewFboWidth + margin) * 4;
	largeH = (mParameterBag->mPreviewFboHeight + margin) * 5;
	largePreviewW = mParameterBag->mPreviewWidth + margin;
	largePreviewH = (mParameterBag->mPreviewHeight + margin) * 2.4;
	displayHeight = mParameterBag->mMainDisplayHeight - 50;
	mouseGlobal = false;
	showConsole = showGlobal = showTextures = showAudio = showMidi = showChannels = showShaders = true;
	showTest = showTheme = showOSC = showFbos = false;
	// set ui window and io events callbacks
	ui::initialize(ui::Options().NewFrameInMainApp(true));
	unsigned char* pixels;
	int width, height;
	ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels);

	// Store our identifier
	ImGui::GetIO().Fonts->TexID = (void *)(intptr_t)tex_id;

	// warping
	mUseBeginEnd = false;
	updateWindowTitle();
	disableFrameRate();

	// initialize warps
	mSettings = getAssetPath("") / "warps.xml";
	if (fs::exists(mSettings)) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings(loadFile(mSettings));
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back(WarpBilinear::create());
		mWarps.push_back(WarpPerspective::create());
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}

	// load test image
	try {
		mImage = gl::Texture::create(loadImage(loadAsset("help.jpg")),
			gl::Texture2d::Format().loadTopDown().mipmap(true).minFilter(GL_LINEAR_MIPMAP_LINEAR));

		mSrcArea = mImage->getBounds();

		// adjust the content size of the warps
		Warp::setSize(mWarps, mImage->getSize());
	}
	catch (const std::exception &e) {
		console() << e.what() << std::endl;
	}
}

void ReymentaServerApp::cleanup()
{
	// save warp settings
	Warp::writeSettings(mWarps, writeFile(mSettings));
	CI_LOG_V("shutdown");

	// save warp settings
	//mBatchass->getWarpsRef()->save();
	// save params
	mParameterBag->save();
	ui::Shutdown();
}
void ReymentaServerApp::fileDrop(FileDropEvent event)
{
	int index;
	string ext = "";
	// use the last of the dropped files
	const fs::path &mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	int dotIndex = mFile.find_last_of(".");
	int slashIndex = mFile.find_last_of("\\");

	if (dotIndex != std::string::npos && dotIndex > slashIndex) ext = mFile.substr(mFile.find_last_of(".") + 1);
	index = (int)(event.getX() / (margin + mParameterBag->mPreviewFboWidth + inBetween));// +1;
	//mBatchass->log(mFile + " dropped, currentSelectedIndex:" + toString(mParameterBag->currentSelectedIndex) + " x: " + toString(event.getX()) + " PreviewFboWidth: " + toString(mParameterBag->mPreviewFboWidth));

	if (ext == "wav" || ext == "mp3")
	{
		//mAudio->loadWaveFile(mFile);
	}
	else if (ext == "png" || ext == "jpg")
	{
		if (index < 1) index = 1;
		if (index > 3) index = 3;
		//mTextures->loadImageFile(mParameterBag->currentSelectedIndex, mFile);
		mBatchass->getTexturesRef()->loadImageFile(index, mFile);
	}
	else if (ext == "glsl")
	{
		if (index < 4) index = 4;
		int rtn = mBatchass->getShadersRef()->loadPixelFragmentShaderAtIndex(mFile, index);
		if (rtn > -1 && rtn < mBatchass->getShadersRef()->getCount())
		{
			mParameterBag->controlValues[22] = 1.0f;
			// TODO  send content via websockets
			/*fs::path fr = mFile;
			string name = "unknown";
			if (mFile.find_last_of("\\") != std::string::npos) name = mFile.substr(mFile.find_last_of("\\") + 1);
			if (fs::exists(fr))
			{

			std::string fs = loadString(loadFile(mFile));
			if (mParameterBag->mOSCEnabled) mOSC->sendOSCStringMessage("/fs", 0, fs, name);
			}*/
			// save thumb
			//timeline().apply(&mTimer, 1.0f, 1.0f).finishFn([&]{ saveThumb(); });
		}
	}
	else if (ext == "mov" || ext == "mp4")
	{
		/*
		if (index < 1) index = 1;
		if (index > 3) index = 3;
		mBatchass->getTexturesRef()->loadMovieFile(index, mFile);*/
	}
	else if (ext == "fs")
	{
		//mShaders->incrementPreviewIndex();
		mBatchass->getShadersRef()->loadFragmentShader(mPath);
	}
	else if (ext == "xml")
	{
		mBatchass->getWarpsRef()->loadWarps(mFile);
	}
	else if (ext == "patchjson")
	{
		// try loading patch
		try
		{
			JsonTree patchjson;
			try
			{
				patchjson = JsonTree(loadFile(mFile));
				mParameterBag->mCurrentFilePath = mFile;
			}
			catch (cinder::JsonTree::Exception exception)
			{
				CI_LOG_V("patchjsonparser exception " + mFile + ": " + exception.what());

			}
			//Assets
			int i = 1; // 0 is audio
			JsonTree jsons = patchjson.getChild("assets");
			for (JsonTree::ConstIter jsonElement = jsons.begin(); jsonElement != jsons.end(); ++jsonElement)
			{
				string jsonFileName = jsonElement->getChild("filename").getValue<string>();
				int channel = jsonElement->getChild("channel").getValue<int>();
				if (channel < mBatchass->getTexturesRef()->getTextureCount())
				{
					CI_LOG_V("asset filename: " + jsonFileName);
					mBatchass->getTexturesRef()->setTexture(channel, jsonFileName);
				}
				i++;
			}

		}
		catch (...)
		{
			CI_LOG_V("patchjson parsing error: " + mFile);
		}
	}
	else if (ext == "txt")
	{
		// try loading shader parts
		if (mBatchass->getShadersRef()->loadTextFile(mFile))
		{

		}
	}
	else if (ext == "")
	{
		// try loading image sequence from dir
		if (index < 1) index = 1;
		if (index > 3) index = 3;
		mBatchass->getTexturesRef()->createFromDir(mFile + "/", index);
		// or create thumbs from shaders
		mBatchass->getShadersRef()->createThumbsFromDir(mFile + "/");
	}
}

void ReymentaServerApp::update()
{
	mParameterBag->iFps = getAverageFps();
	mParameterBag->sFps = toString(floor(mParameterBag->iFps));
	getWindow()->setTitle("(" + mParameterBag->sFps + " fps) Server");
	if (mParameterBag->iGreyScale)
	{
		mParameterBag->controlValues[1] = mParameterBag->controlValues[2] = mParameterBag->controlValues[3];
		mParameterBag->controlValues[5] = mParameterBag->controlValues[6] = mParameterBag->controlValues[7];
	}

	mParameterBag->iChannelTime[0] = getElapsedSeconds();
	mParameterBag->iChannelTime[1] = getElapsedSeconds() - 1;
	mParameterBag->iChannelTime[3] = getElapsedSeconds() - 2;
	mParameterBag->iChannelTime[4] = getElapsedSeconds() - 3;
	//
	if (mParameterBag->mUseTimeWithTempo)
	{
		mParameterBag->iGlobalTime = mParameterBag->iTempoTime*mParameterBag->iTimeFactor;
	}
	else
	{
		mParameterBag->iGlobalTime = getElapsedSeconds();
	}
	mParameterBag->iGlobalTime *= mParameterBag->iSpeedMultiplier;

	if (ui::GetDrawData() != NULL) {
		// Count
		int cmd_count = 0;
		int vtx_count = 0;
		for (int n = 0; n < ui::GetDrawData()->CmdListsCount; n++)
		{
			const ImDrawList * cmd_list = ui::GetDrawData()->CmdLists[n];
			const ImDrawVert * vtx_src = cmd_list->VtxBuffer.begin();
			cmd_count += cmd_list->CmdBuffer.size();
			vtx_count += cmd_list->VtxBuffer.size();
		}

		// Send 
		static int sendframe = 0;
		if (sendframe++ >= 12) // every 2 frames, @TWEAK
		{
			sendframe = 0;
			mBatchass->preparePacketFrame(cmd_count, vtx_count);
			// Add all drawcmds
			Cmd cmd;
			for (int n = 0; n < ui::GetDrawData()->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = ui::GetDrawData()->CmdLists[n];
				const ImDrawCmd* pcmd_end = cmd_list->CmdBuffer.end();
				for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != pcmd_end; pcmd++)
				{
					cmd.Set(*pcmd);
					mBatchass->Write(cmd);
				}
			}
			// Add all vtx
			Vtx vtx;
			for (int n = 0; n < ui::GetDrawData()->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = ui::GetDrawData()->CmdLists[n];
				const ImDrawVert* vtx_src = cmd_list->VtxBuffer.begin();
				int vtx_remaining = cmd_list->VtxBuffer.size();
				while (vtx_remaining-- > 0)
				{
					vtx.Set(*vtx_src++);
					mBatchass->Write(vtx);
				}
			}
			// Send
			mBatchass->SendPacket();
		}
	}
	ImGuiIO& io = ImGui::GetIO();

	// Setup resolution (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;

	// Setup time step
	static double time = 0.0f;
	const double current_time = getElapsedSeconds();
	io.DeltaTime = (float)(current_time - time);
	time = current_time;

	// @RemoteImgui begin
	/*ImGui::RemoteUpdate();
	ImGui::RemoteInput input;
	if (ImGui::RemoteGetInput(input))
	{
		ImGuiIO& io = ImGui::GetIO();
		for (int i = 0; i < 256; i++)
			io.KeysDown[i] = input.KeysDown[i];
		io.KeyCtrl = input.KeyCtrl;
		io.KeyShift = input.KeyShift;
		io.MousePos = input.MousePos;
		io.MouseDown[0] = (input.MouseButtons & 1);
		io.MouseDown[1] = (input.MouseButtons & 2) != 0;
		io.MouseWheel = (float)input.MouseWheel;
	}
	else*/
		// @RemoteImgui end
	{
		// Setup inputs
		// (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
		//double mouse_x, mouse_y;
		//glfwGetCursorPos(window, &mouse_x, &mouse_y);
		//mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
		//mouse_y *= (float)display_h / h;
		//io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
		//io.MouseDown[0] = mousePressed[0] || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
		//io.MouseDown[1] = mousePressed[1] || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != 0;
	}
	ui::NewFrame();
	mBatchass->update();
}

void ReymentaServerApp::draw()
{
	// clear the window and set the drawing color to white
	gl::clear();
	gl::color(Color::white());
	// draw the fbos
	mBatchass->getTexturesRef()->draw();
	gl::enableAlphaBlending();
	//gl::setMatricesWindow(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);
	if (mImage) {
		// iterate over the warps and draw their content
		for (auto &warp : mWarps) {
			// there are two ways you can use the warps:
			if (mUseBeginEnd) {
				// a) issue your draw commands between begin() and end() statements
				warp->begin();

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: gl::draw( mImage );
				gl::draw(mImage, mSrcArea, warp->getBounds());

				warp->end();
			}
			else {
				// b) simply draw a texture on them (ideal for video)

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: warp->draw( mImage );
				warp->draw(mImage, mSrcArea);
			}
		}
	}
	gl::disableAlphaBlending();

	//imgui
	gl::setMatricesWindow(getWindowSize());
	xPos = margin;
	yPos = margin;
	const char* warpInputs[] = { "mix", "left", "right", "warp1", "warp2", "preview", "abp", "live", "w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15" };
	ui::ScopedMainMenuBar mainMenu;

	// File Menu
	if (ui::BeginMenu("File")) {
		ui::MenuItem("New");
			
		ui::MenuItem("Open");
		ui::MenuItem("Save");
		ui::MenuItem("Save As");

		ui::EndMenu();
	}
#pragma region Info

	ui::SetNextWindowSize(ImVec2(largePreviewW + 20, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiSetCond_Once);
	sprintf_s(buf, "Fps %c %d###fps", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3], (int)mParameterBag->iFps);
	ui::Begin(buf);
	{
		ImGui::PushItemWidth(mParameterBag->mPreviewFboWidth);
		// fps
		static ImVector<float> values; if (values.empty()) { values.resize(100); memset(&values.front(), 0, values.size()*sizeof(float)); }
		static int values_offset = 0;
		static float refresh_time = -1.0f;
		if (ui::GetTime() > refresh_time + 1.0f / 6.0f)
		{
			refresh_time = ui::GetTime();
			values[values_offset] = mParameterBag->iFps;
			values_offset = (values_offset + 1) % values.size();
		}
		if (mParameterBag->iFps < 12.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
		ui::PlotLines("FPS", &values.front(), (int)values.size(), values_offset, mParameterBag->sFps.c_str(), 0.0f, 300.0f, ImVec2(0, 30));
		if (mParameterBag->iFps < 12.0) ui::PopStyleColor();

		// Checkbox
		ui::Checkbox("Tex", &showTextures);
		ui::SameLine();
		ui::Checkbox("Fbos", &showFbos);
		ui::SameLine();
		ui::Checkbox("Shada", &showShaders);

		ui::Checkbox("Audio", &showAudio);
		ui::SameLine();
		ui::Checkbox("Cmd", &showConsole);
		ui::SameLine();
		ui::Checkbox("OSC", &showOSC);

		ui::Checkbox("MIDI", &showMidi);
		ui::SameLine();
		ui::Checkbox("Test", &showTest);
		if (ui::Button("Save Params"))
		{
			// save warp settings
			mBatchass->getWarpsRef()->save("warps1.xml");
			// save params
			mParameterBag->save();
		}

		mParameterBag->iDebug ^= ui::Button("Debug");
		ui::SameLine();
		mParameterBag->mRenderThumbs ^= ui::Button("Thumbs");
		ui::PopItemWidth();
		if (ui::Button("Stop Loading")) mBatchass->stopLoading();
	}
	ui::End();
	xPos += largePreviewW + 20 + margin;

#pragma endregion Info


#pragma region MIDI

	// MIDI window
	if (showMidi)
	{
		ui::SetNextWindowSize(ImVec2(largePreviewW + 20, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiSetCond_Once);
		ui::Begin("MIDI");
		{
			sprintf_s(buf, "Enable");
			if (ui::Button(buf)) mBatchass->midiSetup();
			if (ui::CollapsingHeader("MidiIn", "20", true, true))
			{
				ui::Columns(2, "data", true);
				ui::Text("Name"); ui::NextColumn();
				ui::Text("Connect"); ui::NextColumn();
				ui::Separator();

				for (int i = 0; i < mBatchass->midiInCount(); i++)
				{
					ui::Text(mBatchass->midiInPortName(i).c_str()); ui::NextColumn();

					if (mBatchass->midiInConnected(i))
					{
						sprintf_s(buf, "Disconnect %d", i);
					}
					else
					{
						sprintf_s(buf, "Connect %d", i);
					}

					if (ui::Button(buf))
					{
						if (mBatchass->midiInConnected(i))
						{
							mBatchass->midiInClosePort(i);
						}
						else
						{
							mBatchass->midiInOpenPort(i);
						}
					}
					ui::NextColumn();
					ui::Separator();
				}
				ui::Columns(1);
			}
		}
		ui::End();
		xPos += largePreviewW + 20 + margin;
		yPos = margin;

	}
#pragma endregion MIDI

#pragma region OSC

	if (showOSC && mParameterBag->mOSCEnabled)
	{
		ui::SetNextWindowSize(ImVec2(largeW, largeH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiSetCond_Once);
		ui::Begin("OSC router");
		{
			ui::Text("Sending to host %s", mParameterBag->mOSCDestinationHost.c_str());
			ui::SameLine();
			ui::Text(" on port %d", mParameterBag->mOSCDestinationPort);
			ui::Text("Sending to 2nd host %s", mParameterBag->mOSCDestinationHost2.c_str());
			ui::SameLine();
			ui::Text(" on port %d", mParameterBag->mOSCDestinationPort2);
			ui::Text(" Receiving on port %d", mParameterBag->mOSCReceiverPort);

			static char str0[128] = "/live/play";
			static int i0 = 0;
			static float f0 = 0.0f;
			ui::InputText("address", str0, IM_ARRAYSIZE(str0));
			ui::InputInt("track", &i0);
			ui::InputFloat("clip", &f0, 0.01f, 1.0f);
			if (ui::Button("Send")) { mBatchass->sendOSCIntMessage(str0, i0); }
		}
		ui::End();
		xPos += largeW + margin;
	}
#pragma endregion OSC

#pragma region WebSockets
	// websockets
	ui::SetNextWindowSize(ImVec2(largeW, largeH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiSetCond_Once);
	ui::Begin("WebSockets server");
	{
		if (mParameterBag->mIsWebSocketsServer)
		{
			ui::Text("WS Server %d", mParameterBag->mWebSocketsPort);
			ui::Text("IP %s", mParameterBag->mWebSocketsHost.c_str());
		}
		else
		{
			ui::Text("WS Client %d", mParameterBag->mWebSocketsPort);
			ui::Text("IP %s", mParameterBag->mWebSocketsHost.c_str());
		}
		if (ui::Button("Connect")) { mBatchass->wsConnect(); }
		ui::SameLine();
		if (ui::Button("Ping")) { mBatchass->wsPing(); }
	}

	ui::End();
	xPos += largeW + margin;
#pragma endregion WebSockets



	// console
	if (showConsole)
	{
		ui::SetNextWindowSize(ImVec2((w + margin) * mParameterBag->MAX, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiSetCond_Once);
		ShowAppConsole(&showConsole);
		if (mParameterBag->newMsg)
		{
			mParameterBag->newMsg = false;
			mConsole->AddLog(mParameterBag->mMsg.c_str());
		}
	}
	if (showTest)
	{
		ui::ShowTestWindow();
		ui::ShowStyleEditor();
	}
}
// From imgui by Omar Cornut
void ReymentaServerApp::ShowAppConsole(bool* opened)
{
	mConsole->Run("Console", opened);
}
void ReymentaServerApp::resize()
{
	// tell the warps our window has been resized, so they properly scale up or down
	Warp::handleResize(mWarps);
}

void ReymentaServerApp::mouseMove(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseMove(mWarps, event)) {
		// let your application perform its mouseMove handling here
	}
}

void ReymentaServerApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
	}
}

void ReymentaServerApp::mouseDrag(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDrag(mWarps, event)) {
		// let your application perform its mouseDrag handling here
	}
}

void ReymentaServerApp::mouseUp(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseUp(mWarps, event)) {
		// let your application perform its mouseUp handling here
	}
}

void ReymentaServerApp::keyDown(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyDown(mWarps, event)) {
		// warp editor did not handle the key, so handle it here
		switch (event.getCode()) {
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_f:
			// toggle full screen
			setFullScreen(!isFullScreen());
			break;
		case KeyEvent::KEY_v:
			// toggle vertical sync
			gl::enableVerticalSync(!gl::isVerticalSyncEnabled());
			break;
		case KeyEvent::KEY_w:
			// toggle warp edit mode
			Warp::enableEditMode(!Warp::isEditModeEnabled());
			break;
		case KeyEvent::KEY_a:
			// toggle drawing a random region of the image
			if (mSrcArea.getWidth() != mImage->getWidth() || mSrcArea.getHeight() != mImage->getHeight())
				mSrcArea = mImage->getBounds();
			else {
				int x1 = Rand::randInt(0, mImage->getWidth() - 150);
				int y1 = Rand::randInt(0, mImage->getHeight() - 150);
				int x2 = Rand::randInt(x1 + 150, mImage->getWidth());
				int y2 = Rand::randInt(y1 + 150, mImage->getHeight());
				mSrcArea = Area(x1, y1, x2, y2);
			}
			break;
		case KeyEvent::KEY_SPACE:
			// toggle drawing mode
			mUseBeginEnd = !mUseBeginEnd;
			updateWindowTitle();
			break;
		}
	}
}

void ReymentaServerApp::keyUp(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyUp(mWarps, event)) {
		// let your application perform its keyUp handling here
	}
}

void ReymentaServerApp::updateWindowTitle()
{
	if (mUseBeginEnd)
		getWindow()->setTitle("Warping Sample - Using begin() and end()");
	else
		getWindow()->setTitle("Warping Sample - Using draw()");
}

CINDER_APP(ReymentaServerApp, RendererGl(RendererGl::Options().msaa(8)), &ReymentaServerApp::prepare)
