//========================================================
// Main source: win32_engine.exe
// Creator: Jordan
// S: C++
//========================================================

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

// TODO(jordan): put these in some sort of resource file
#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

struct win32_offscreen_buffer
{
	// NOTE(jordan): Pixels are always 32-bits wide. Memory order BB GG RR XX
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};


struct win32_window_dimension
{
	int Width;
	int Height;
};

// TODO(jordan): this is golbal for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

// NOTE(jordan): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(jordan): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
	// NOTE(jordan): test this on a windows 8 machine
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if(DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
		  GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;					
			WaveFormat.cbSize = 0;

			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE(jordan): create a primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if(SUCCEEDED(Error))
					{
						// NOTE(jordan): the format has been set of direct sound
						OutputDebugStringA("Primary buffer format was set.\n");
					}
					else
					{
						// TODO(jordan): diagnostic
					}

				}
				else
				{
					// TODO(jordan): diagnostic
				}
			}
			else
			{
				// TODO(jordan): Diagnostic
			}

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			LPDIRECTSOUNDBUFFER SecondaryBuffer;
			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0);
			if(SUCCEEDED(Error))
			{
				OutputDebugStringA("Secondary Buffer created successfully. \n");
			}

			// NOTE(jordan): start playing
		}
		else
		{
			// TODO(jordan): Diagnostics
		}
	}
	else
	{
		// TODO(jordan): Diagnostics
	}

}

win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
}

internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	// TODO(jordan): lets see what the optimizer does for this
	
	uint8 *Row = (uint8 *)Buffer->Memory;

	for(int Y = 0; 
		Y < Buffer->Height;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0;
			X < Buffer->Width;
			++X)
		{
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);

			/*
				Pixel (32 - bits)
				
				Memory:		BB GG RR xx
				Register: 	XX RR GG BB

				Or the bits of blue with the bits of green.
				Move them up to where they would be, which is 8 bits up.
				So I shift them 8 bits and OR them together to my Pixel.
			*/

			*Pixel++ = ((Green << 8) | Blue);
		}

		Row += Buffer->Pitch;
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	// TODO(jordan): bulletproof this
	// Maybe don't free first, free after, then free first if that fails
	
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	// TODO(jordan): probly want to clear this to black

	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,						   
						   HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	// TODO(jordan) Aspect Ratio Correction
	StretchDIBits(DeviceContext, 
					/*
					X, Y, Width, Height,
					X, Y, Width, Height,
					*/
					0, 0, WindowWidth, WindowHeight,
					0, 0, Buffer->Width, Buffer->Height,
					Buffer->Memory,
					&Buffer->Info,
					DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND   Window,
  				   UINT   Message,
  				   WPARAM WParam,
  				   LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case  WM_CLOSE:
		{
			//TODO(jordan): handle this with a message to the user?
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_DESTROY:
		{
			//TODO(jordan): handle this as an error - recreate window?
			GlobalRunning = false;
		} break;
	
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) == 0);

			if(WasDown != IsDown)
			{
				if(VKCode == 'W')
				{
				}
				else if(VKCode == 'A')
				{
				}
				else if(VKCode == 'S')
				{
				}
				else if(VKCode == 'D')
				{
				}
				else if(VKCode == 'Q')
				{
				}
				else if(VKCode == 'E')
				{
				}
				else if(VKCode == VK_UP)
				{
				}
				else if(VKCode == VK_LEFT)
				{
				}
				else if(VKCode == VK_DOWN)
				{
				}
				else if(VKCode == VK_RIGHT)
				{
				}
				else if(VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("IsDown");
					}
					if(WasDown)
					{
						OutputDebugStringA("WasDown");
					}
					OutputDebugStringA("\n");
				}
				else if(VKCode == VK_SPACE)
				{
				}
			}

			bool32 AltKeyWasDown = ((LParam & (1 << 29)) != 0);
			if((VKCode == VK_F4) && AltKeyWasDown)
			{
				GlobalRunning = false;
			}
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, 
									   Dimension.Width, Dimension.Height);
			
			EndPaint(Window, &Paint);

		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}

int 	CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)
{
	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
  	WindowClass.lpfnWndProc = Win32MainWindowCallback;
  	WindowClass.hInstance = Instance;
  	//HICON     hIcon;
  	WindowClass.lpszClassName = "EngineWindowClass";

  	if(RegisterClass(&WindowClass))
  	{
  		HWND Window = 
  			CreateWindowExA(
  				0,
  				WindowClass.lpszClassName,
  				"Engine v0.1",
  				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
  				CW_USEDEFAULT,
  				CW_USEDEFAULT,
  				CW_USEDEFAULT,
  				CW_USEDEFAULT,
  				0,
  				0,
  				Instance,
  				0);
  			if(Window)
  			{
  				// NOTE(jordan): since we specified CS_OWNDC, we can just
  				// get one device context and use it forever because we
  				// are not sharing it with anyone.
  				HDC DeviceContext = GetDC(Window);

  				int XOffset = 0;
  				int YOffset = 0;

  				Win32InitDSound(Window, 48000, 48000*sizeof(int16)*2);

  				GlobalRunning = true;
  				while(GlobalRunning)
  				{
  					MSG Message;
  					while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
  					{
  						if(Message.message == WM_QUIT)
  						{
  							GlobalRunning = false;
  						}

  						TranslateMessage(&Message);
  						DispatchMessage(&Message);
  					}

  					// TODO(jordan): should we poll this more frequently?
  					for(DWORD ControllerIndex = 0;
  						ControllerIndex <  XUSER_MAX_COUNT;
  						++ControllerIndex)
  					{
  						XINPUT_STATE ControllerState;
  						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
  						{
  							// NOTE(jordan): this controller is plugged in
  							// TODO(jordan): see if ControllerState.dwPacketNumber incrememnts too rapidly for us to care
  							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

  							bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
  							bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
  							bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
  							bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
  							bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
  							bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
  							bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
  							bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
  							bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
  							bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
  							bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
  							bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

  							int16 StickX = Pad->sThumbLX;
  							int16 StickY = Pad->sThumbLY;

  							if(AButton)
  							{
  								YOffset += 2;
  							}
  						}
  						else
  						{
  							// NOTE(jordan): this controller is not plugged in
  						}
  					}

  					
  					/*XINPUT_VIBRATION Vibration;
  					Vibration.wLeftMotorSpeed = 60000;
  					Vibration.wRightMotorSpeed = 60000;
  					XInputSetState(0, &Vibration); */

					RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
											   Dimension.Width, Dimension.Height);
					++XOffset;
  				}
  			}
  			else
  			{
  				//TODO(jordan): Logger system
  			}
  	}
  	else
  	{
  		// TODO(jordan): logger system
  	}

	return (0);
}