#include <AudioToolbox/AudioToolbox.h>
#include "Chip700.h"
#include "brrcodec.h"
#include "AudioFile.h"
#include "XIFile.h"

static CFStringRef kSaveKey_ProgName = CFSTR("progname");
static CFStringRef kSaveKey_EditProg = CFSTR("editprog");
static CFStringRef kSaveKey_EditChan = CFSTR("editchan");
static CFStringRef kSaveKey_brrdata = CFSTR("brrdata");
static CFStringRef kSaveKey_looppoint = CFSTR("looppoint");
static CFStringRef kSaveKey_samplerate = CFSTR("samplerate");
static CFStringRef kSaveKey_basekey = CFSTR("key");
static CFStringRef kSaveKey_lowkey = CFSTR("lowkey");
static CFStringRef kSaveKey_highkey = CFSTR("highkey");
static CFStringRef kSaveKey_ar = CFSTR("ar");
static CFStringRef kSaveKey_dr = CFSTR("dr");
static CFStringRef kSaveKey_sl = CFSTR("sl");
static CFStringRef kSaveKey_sr = CFSTR("sr");
static CFStringRef kSaveKey_volL = CFSTR("volL");
static CFStringRef kSaveKey_volR = CFSTR("volR");
static CFStringRef kSaveKey_echo = CFSTR("echo");
static CFStringRef kSaveKey_bank = CFSTR("bank");
static CFStringRef kSaveKey_IsEmphasized = CFSTR("isemph");
static CFStringRef kSaveKey_SourceFile = CFSTR("srcfile");

//-----------------------------------------------------------------------------

COMPONENT_ENTRY(Chip700)

//-----------------------------------------------------------------------------
//	Chip700::Chip700
//-----------------------------------------------------------------------------
Chip700::Chip700(AudioUnit component)
: AUInstrumentBase(component, 0, 1)
{
	CreateElements();
	Globals()->UseIndexedParameters(kNumberOfParameters);
	
	mEfx = new C700Kernel();
	mEfx->SetPropertyNotifyFunc(PropertyNotifyFunc, this);
	mEfx->SetParameterSetFunc(ParameterSetFunc, this);
	
	//プリセット名テーブルを作成する
	mPresets = new AUPreset[NUM_PRESETS];
	for (int i = 0; i < NUM_PRESETS; ++i) {
		const char		*pname;
		pname = C700Kernel::GetPresetName(i);
		mPresets[i].presetNumber = i;
		CFMutableStringRef cfpname = CFStringCreateMutable(NULL, 64);
		mPresets[i].presetName = cfpname;
		CFStringAppendCString( cfpname, pname, kCFStringEncodingASCII );
    }
	
	for ( int i=0; i<kNumberOfParameters; i++ ) {
		Globals()->SetParameter(i, C700Kernel::GetParameterDefault(i) );
	}
	
#if AU_DEBUG_DISPATCHER
	mDebugDispatcher = new AUDebugDispatcher(this);
#endif
}

//-----------------------------------------------------------------------------
Chip700::~Chip700()
{
	for (int i = 0; i < NUM_PRESETS; ++i) {
		CFRelease(mPresets[i].presetName);
	}
	delete [] mPresets;
	
#if AU_DEBUG_DISPATCHER
	delete mDebugDispatcher;
#endif
}

//-----------------------------------------------------------------------------
void Chip700::PropertyNotifyFunc(int propID, void* userData)
{
	Chip700	*This = reinterpret_cast<Chip700*> (userData);
	This->PropertyChanged(propID, kAudioUnitScope_Global, 0);
}

//-----------------------------------------------------------------------------
void Chip700::ParameterSetFunc(int paramID, float value, void* userData)
{
	Chip700	*This = reinterpret_cast<Chip700*> (userData);
	This->Globals()->SetParameter(paramID, value);
	AudioUnitEvent auEvent;
	auEvent.mEventType = kAudioUnitEvent_ParameterValueChange;
	auEvent.mArgument.mParameter.mAudioUnit = (AudioUnit)This->GetComponentInstance();
	auEvent.mArgument.mParameter.mScope = kAudioUnitScope_Global;
	auEvent.mArgument.mParameter.mParameterID = paramID;
	auEvent.mArgument.mParameter.mElement = 0;
	AUEventListenerNotify(NULL, NULL, &auEvent);
}

//-----------------------------------------------------------------------------
ComponentResult Chip700::Initialize()
{	
	AUInstrumentBase::Initialize();
	return noErr;
}

//-----------------------------------------------------------------------------
void Chip700::Cleanup()
{
	delete mEfx;
}

//-----------------------------------------------------------------------------
ComponentResult Chip700::Reset(	AudioUnitScope 		inScope,
							   AudioUnitElement 	inElement)
{
//	if (inScope == kAudioUnitScope_Global) {
		mEfx->Reset();
//	}	
	return AUInstrumentBase::Reset(inScope, inElement);
}

//-----------------------------------------------------------------------------
OSStatus	Chip700::Render(   AudioUnitRenderActionFlags &	ioActionFlags,
												const AudioTimeStamp &			inTimeStamp,
												UInt32							inNumberFrames)
{
	OSStatus result = AUInstrumentBase::Render(ioActionFlags, inTimeStamp, inNumberFrames);
	
	CallHostBeatAndTempo(NULL, &mTempo);
	mEfx->SetTempo( mTempo );
	//バッファの確保
	float				*output[2];
	AudioBufferList&	bufferList = GetOutput(0)->GetBufferList();
	
	int numChans = bufferList.mNumberBuffers;
	if (numChans > 2) return -1;
	output[0] = (float*)bufferList.mBuffers[0].mData;
	output[1] = numChans==2 ? (float*)bufferList.mBuffers[1].mData : NULL;

	//パラメータの反映
	for ( int i=0; i<kNumberOfParameters; i++ ) {
		mEfx->SetParameter(i, Globals()->GetParameter(i));
	}
	
	mEfx->SetSampleRate( GetOutput(0)->GetStreamFormat().mSampleRate );
	
	mEfx->Render(inNumberFrames, output);
	
	return result;
}

//-----------------------------------------------------------------------------
//	Chip700::SetParameter
//-----------------------------------------------------------------------------
OSStatus Chip700::SetParameter(	AudioUnitParameterID			inID,
								 AudioUnitScope 				inScope,
								 AudioUnitElement 				inElement,
								 Float32						inValue,
								 UInt32							inBufferOffsetInFrames)
{
	OSStatus result = AUInstrumentBase::SetParameter(inID, inScope, inElement, inValue, inBufferOffsetInFrames);
	if ( inScope == kAudioUnitScope_Global ) {
		//mEfx->SetParameter(inID, inValue);
		switch ( inID ) {
			case kParam_echodelay:
				PropertyChanged(kAudioUnitCustomProperty_TotalRAM, kAudioUnitScope_Global, 0);
				
			case kParam_echovol_L:
			case kParam_echovol_R:
			case kParam_echoFB:
			case kParam_fir0:
			case kParam_fir1:
			case kParam_fir2:
			case kParam_fir3:
			case kParam_fir4:
			case kParam_fir5:
			case kParam_fir6:
			case kParam_fir7:
				PropertyChanged(kAudioUnitCustomProperty_Band1, kAudioUnitScope_Global, 0);
				break;
		}
	}
	return result;
}

//-----------------------------------------------------------------------------
AudioUnitParameterUnit getParameterUnit( int id )
{
	switch(id)
	{
		case kParam_poly:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_vibdepth:
		case kParam_vibdepth_2:
		case kParam_vibdepth_3:
		case kParam_vibdepth_4:
		case kParam_vibdepth_5:
		case kParam_vibdepth_6:
		case kParam_vibdepth_7:
		case kParam_vibdepth_8:
		case kParam_vibdepth_9:
		case kParam_vibdepth_10:
		case kParam_vibdepth_11:
		case kParam_vibdepth_12:
		case kParam_vibdepth_13:
		case kParam_vibdepth_14:
		case kParam_vibdepth_15:
		case kParam_vibdepth_16:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_vibrate:
			return kAudioUnitParameterUnit_Hertz;
		case kParam_vibdepth2:
			return kAudioUnitParameterUnit_Generic;
		case kParam_velocity:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_clipnoise:
			return kAudioUnitParameterUnit_Boolean;
		case kParam_bendrange:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_program:
		case kParam_program_2:
		case kParam_program_3:
		case kParam_program_4:
		case kParam_program_5:
		case kParam_program_6:
		case kParam_program_7:
		case kParam_program_8:
		case kParam_program_9:
		case kParam_program_10:
		case kParam_program_11:
		case kParam_program_12:
		case kParam_program_13:
		case kParam_program_14:
		case kParam_program_15:
		case kParam_program_16:
			return kAudioUnitParameterUnit_Indexed;
			
		case kParam_bankAmulti:
			return kAudioUnitParameterUnit_Boolean;
		case kParam_bankBmulti:
			return kAudioUnitParameterUnit_Boolean;
		case kParam_bankCmulti:
			return kAudioUnitParameterUnit_Boolean;
		case kParam_bankDmulti:
			return kAudioUnitParameterUnit_Boolean;
			
			//エコー
		case kParam_mainvol_L:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_mainvol_R:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_echovol_L:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_echovol_R:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_echoFB:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_echodelay:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir0:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir1:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir2:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir3:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir4:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir5:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir6:
			return kAudioUnitParameterUnit_Indexed;
		case kParam_fir7:
			return kAudioUnitParameterUnit_Indexed;
			
		default:
			return kAudioUnitParameterUnit_Indexed;
	}
}
//-----------------------------------------------------------------------------
//	Chip700::GetParameterInfo
//-----------------------------------------------------------------------------
ComponentResult		Chip700::GetParameterInfo(AudioUnitScope		inScope,
											  AudioUnitParameterID	inParameterID,
											  AudioUnitParameterInfo	&outParameterInfo )
{
	ComponentResult result = noErr;
	
	outParameterInfo.flags = 	kAudioUnitParameterFlag_IsWritable
		|		kAudioUnitParameterFlag_IsReadable;
    
    if (inScope == kAudioUnitScope_Global) {
		if ( inParameterID >= 0 && inParameterID < kNumberOfParameters ) {
			CFStringRef	cfName = CFStringCreateWithCString(NULL, mEfx->GetParameterName(inParameterID), kCFStringEncodingUTF8);
			AUBase::FillInParameterName(outParameterInfo, cfName, true);
			outParameterInfo.unit = getParameterUnit( inParameterID );
			outParameterInfo.minValue = C700Kernel::GetParameterMin(inParameterID);
			outParameterInfo.maxValue = C700Kernel::GetParameterMax(inParameterID);
			outParameterInfo.defaultValue = C700Kernel::GetParameterDefault(inParameterID);
		}
		else {
			result = kAudioUnitErr_InvalidParameter;
		}
	}
	else {
        result = kAudioUnitErr_InvalidParameter;
    }

	return result;
}

//-----------------------------------------------------------------------------
//	Chip700::GetPropertyInfo
//-----------------------------------------------------------------------------
ComponentResult		Chip700::GetPropertyInfo (AudioUnitPropertyID	inID,
											  AudioUnitScope		inScope,
											  AudioUnitElement	inElement,
											  UInt32 &		outDataSize,
											  Boolean &		outWritable)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
#ifndef USE_CARBON_UI
			case kAudioUnitProperty_CocoaUI:
				outWritable = false;
				outDataSize = sizeof (AudioUnitCocoaViewInfo);
				return noErr;
#endif
			
			case kAudioUnitCustomProperty_BaseKey:
			case kAudioUnitCustomProperty_LowKey:
			case kAudioUnitCustomProperty_HighKey:
			case kAudioUnitCustomProperty_AR:
			case kAudioUnitCustomProperty_DR:
			case kAudioUnitCustomProperty_SL:
			case kAudioUnitCustomProperty_SR:
			case kAudioUnitCustomProperty_VolL:
			case kAudioUnitCustomProperty_VolR:
			case kAudioUnitCustomProperty_EditingProgram:
			case kAudioUnitCustomProperty_EditingChannel:
			case kAudioUnitCustomProperty_LoopPoint:
				outDataSize = sizeof(int);
				outWritable = false;
				return noErr;
				
			case kAudioUnitCustomProperty_Rate:
				outDataSize = sizeof(double);
				outWritable = false;
				return noErr;
			
			case kAudioUnitCustomProperty_Loop:
				outDataSize = sizeof(bool);
				outWritable = false;
				return noErr;
			
			case kAudioUnitCustomProperty_Echo:
				outDataSize = sizeof(bool);
				outWritable = false;
				return noErr;

			case kAudioUnitCustomProperty_Bank:
				outDataSize = sizeof(UInt32);
				outWritable = false;
				return noErr;
				
			case kAudioUnitCustomProperty_BRRData:
				outDataSize = sizeof(BRRData);
				outWritable = false;
				return noErr;
				
			case kAudioUnitCustomProperty_PGDictionary:
				outDataSize = sizeof(CFDictionaryRef);
				outWritable = false;
				return noErr;
				
			case kAudioUnitCustomProperty_XIData:
				outDataSize = sizeof(CFDataRef);
				outWritable = false;
				return noErr;
			
			case kAudioUnitCustomProperty_ProgramName:
				outDataSize = sizeof(CFStringRef);
				outWritable = false;
				return noErr;
				
			case kAudioUnitCustomProperty_TotalRAM:
				outDataSize = sizeof(UInt32);
				outWritable = false;
				return noErr;
			
			//エコー
			case kAudioUnitCustomProperty_Band1:
			case kAudioUnitCustomProperty_Band2:
			case kAudioUnitCustomProperty_Band3:
			case kAudioUnitCustomProperty_Band4:
			case kAudioUnitCustomProperty_Band5:
				outWritable = false;
				outDataSize = sizeof(Float32);
				return noErr;
				
			// トラックインジケーター
			case kAudioUnitCustomProperty_NoteOnTrack_1:
			case kAudioUnitCustomProperty_NoteOnTrack_2:
			case kAudioUnitCustomProperty_NoteOnTrack_3:
			case kAudioUnitCustomProperty_NoteOnTrack_4:
			case kAudioUnitCustomProperty_NoteOnTrack_5:	
			case kAudioUnitCustomProperty_NoteOnTrack_6:
			case kAudioUnitCustomProperty_NoteOnTrack_7:
			case kAudioUnitCustomProperty_NoteOnTrack_8:
			case kAudioUnitCustomProperty_NoteOnTrack_9:
			case kAudioUnitCustomProperty_NoteOnTrack_10:
			case kAudioUnitCustomProperty_NoteOnTrack_11:
			case kAudioUnitCustomProperty_NoteOnTrack_12:
			case kAudioUnitCustomProperty_NoteOnTrack_13:
			case kAudioUnitCustomProperty_NoteOnTrack_14:
			case kAudioUnitCustomProperty_NoteOnTrack_15:
			case kAudioUnitCustomProperty_NoteOnTrack_16:
				outWritable = false;
				outDataSize = sizeof(UInt32);
				return noErr;
				
			//トラック最大発音数
			case kAudioUnitCustomProperty_MaxNoteTrack_1:
			case kAudioUnitCustomProperty_MaxNoteTrack_2:
			case kAudioUnitCustomProperty_MaxNoteTrack_3:
			case kAudioUnitCustomProperty_MaxNoteTrack_4:
			case kAudioUnitCustomProperty_MaxNoteTrack_5:
			case kAudioUnitCustomProperty_MaxNoteTrack_6:
			case kAudioUnitCustomProperty_MaxNoteTrack_7:
			case kAudioUnitCustomProperty_MaxNoteTrack_8:
			case kAudioUnitCustomProperty_MaxNoteTrack_9:
			case kAudioUnitCustomProperty_MaxNoteTrack_10:
			case kAudioUnitCustomProperty_MaxNoteTrack_11:
			case kAudioUnitCustomProperty_MaxNoteTrack_12:
			case kAudioUnitCustomProperty_MaxNoteTrack_13:
			case kAudioUnitCustomProperty_MaxNoteTrack_14:
			case kAudioUnitCustomProperty_MaxNoteTrack_15:
			case kAudioUnitCustomProperty_MaxNoteTrack_16:
				outWritable = false;
				outDataSize = sizeof(UInt32);
				return noErr;
				
			case kAudioUnitCustomProperty_SourceFileRef:
				outDataSize = sizeof(CFURLRef);
				outWritable = false;
				return noErr;
								
			case kAudioUnitCustomProperty_IsEmaphasized:
				outDataSize = sizeof(bool);
				outWritable = false;
				return noErr;
								
		}
	}
	
	return AUInstrumentBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

//-----------------------------------------------------------------------------
//	Chip700::GetProperty
//-----------------------------------------------------------------------------
ComponentResult		Chip700::GetProperty(	AudioUnitPropertyID inID,
											AudioUnitScope 		inScope,
											AudioUnitElement 	inElement,
											void *			outData )
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
#ifndef USE_CARBON_UI
			case kAudioUnitProperty_CocoaUI:
			{
				// Look for a resource in the main bundle by name and type.
				CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR("com.VeMa.audiounit.Chip700") );
				
				if (bundle == NULL) return fnfErr;

				CFURLRef bundleURL = CFBundleCopyBundleURL( bundle );
				
				if (bundleURL == NULL) return fnfErr;
				
				AudioUnitCocoaViewInfo cocoaInfo;
				cocoaInfo.mCocoaAUViewBundleLocation = bundleURL;
				cocoaInfo.mCocoaAUViewClass[0] = CFStringCreateWithCString(NULL, "C700_CocoaViewFactory", kCFStringEncodingUTF8);
				
				*((AudioUnitCocoaViewInfo *)outData) = cocoaInfo;
				
				return noErr;
			}
#endif
			
			case kAudioUnitCustomProperty_Rate:
				*((double *)outData) = mEfx->GetPropertyValue(inID);
				return noErr;
			
			case kAudioUnitCustomProperty_BaseKey:
			case kAudioUnitCustomProperty_LowKey:
			case kAudioUnitCustomProperty_HighKey:
			case kAudioUnitCustomProperty_LoopPoint:
			case kAudioUnitCustomProperty_Bank:
			case kAudioUnitCustomProperty_AR:
			case kAudioUnitCustomProperty_DR:
			case kAudioUnitCustomProperty_SL:
			case kAudioUnitCustomProperty_SR:
			case kAudioUnitCustomProperty_VolL:
			case kAudioUnitCustomProperty_VolR:
			case kAudioUnitCustomProperty_EditingProgram:
			case kAudioUnitCustomProperty_EditingChannel:
			case kAudioUnitCustomProperty_NoteOnTrack_1:
			case kAudioUnitCustomProperty_NoteOnTrack_2:
			case kAudioUnitCustomProperty_NoteOnTrack_3:
			case kAudioUnitCustomProperty_NoteOnTrack_4:
			case kAudioUnitCustomProperty_NoteOnTrack_5:
			case kAudioUnitCustomProperty_NoteOnTrack_6:
			case kAudioUnitCustomProperty_NoteOnTrack_7:
			case kAudioUnitCustomProperty_NoteOnTrack_8:
			case kAudioUnitCustomProperty_NoteOnTrack_9:
			case kAudioUnitCustomProperty_NoteOnTrack_10:
			case kAudioUnitCustomProperty_NoteOnTrack_11:
			case kAudioUnitCustomProperty_NoteOnTrack_12:
			case kAudioUnitCustomProperty_NoteOnTrack_13:
			case kAudioUnitCustomProperty_NoteOnTrack_14:
			case kAudioUnitCustomProperty_NoteOnTrack_15:
			case kAudioUnitCustomProperty_NoteOnTrack_16:
			case kAudioUnitCustomProperty_MaxNoteTrack_1:
			case kAudioUnitCustomProperty_MaxNoteTrack_2:
			case kAudioUnitCustomProperty_MaxNoteTrack_3:
			case kAudioUnitCustomProperty_MaxNoteTrack_4:
			case kAudioUnitCustomProperty_MaxNoteTrack_5:
			case kAudioUnitCustomProperty_MaxNoteTrack_6:
			case kAudioUnitCustomProperty_MaxNoteTrack_7:
			case kAudioUnitCustomProperty_MaxNoteTrack_8:
			case kAudioUnitCustomProperty_MaxNoteTrack_9:
			case kAudioUnitCustomProperty_MaxNoteTrack_10:
			case kAudioUnitCustomProperty_MaxNoteTrack_11:
			case kAudioUnitCustomProperty_MaxNoteTrack_12:
			case kAudioUnitCustomProperty_MaxNoteTrack_13:
			case kAudioUnitCustomProperty_MaxNoteTrack_14:
			case kAudioUnitCustomProperty_MaxNoteTrack_15:
			case kAudioUnitCustomProperty_MaxNoteTrack_16:
				*((int *)outData) = mEfx->GetPropertyValue(inID);
				return noErr;
				
			case kAudioUnitCustomProperty_Loop:
			case kAudioUnitCustomProperty_Echo:
			case kAudioUnitCustomProperty_IsEmaphasized:
				*((bool *)outData) = mEfx->GetPropertyValue(inID)>0.5f? true:false;
				return noErr;
				
			case kAudioUnitCustomProperty_TotalRAM:
				*((UInt32 *)outData) = mEfx->GetPropertyValue(inID);
				return noErr;
							
			//エコー
			case kAudioUnitCustomProperty_Band1:
			case kAudioUnitCustomProperty_Band2:
			case kAudioUnitCustomProperty_Band3:
			case kAudioUnitCustomProperty_Band4:
			case kAudioUnitCustomProperty_Band5:
				*((Float32 *)outData) = mEfx->GetPropertyValue(inID);
				return noErr;
				
			case kAudioUnitCustomProperty_BRRData:
				*((BRRData *)outData) = *(mEfx->GetBRRData());
				return noErr;
				
			case kAudioUnitCustomProperty_PGDictionary:
			{
				CFDictionaryRef	pgdata;
				int				editProg = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingProgram);
				CreatePGDataDic(&pgdata, editProg);
				*((CFDictionaryRef *)outData) = pgdata;	//使用後要release
				return noErr;
			}
				
			case kAudioUnitCustomProperty_XIData:
			{
				XIFile	fileData(NULL);
				
				fileData.SetDataFromChip(mEfx->GetGenerator(), 
										 mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingProgram), mTempo);
				
				if ( fileData.IsLoaded() ) {
					CFDataRef xidata;
					xidata = CFDataCreate(NULL, fileData.GetDataPtr(), fileData.GetWriteSize() );
					*((CFDataRef *)outData) = xidata;	//使用後要release
				}
				else {
					*((CFDataRef *)outData) = NULL;
				}
				return noErr;
			}
				
			case kAudioUnitCustomProperty_ProgramName:
			{
				CFStringRef	str = CFStringCreateWithCString(NULL, mEfx->GetProgramName(), kCFStringEncodingUTF8);
				*((CFStringRef *)outData) = str;	//使用後要release
				return noErr;
			}

			case kAudioUnitCustomProperty_SourceFileRef:
			{
				const char *srcPath = mEfx->GetSourceFilePath();
				CFURLRef	url = 
				CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*)srcPath, strlen(srcPath), false);
				*((CFURLRef *)outData) = url;	//使用後要release
				return noErr;
			}				
				
		}
	}
	return AUInstrumentBase::GetProperty(inID, inScope, inElement, outData);
}

//-----------------------------------------------------------------------------
//	Chip700::SetProperty
//-----------------------------------------------------------------------------
ComponentResult		Chip700::SetProperty(	AudioUnitPropertyID inID,
											AudioUnitScope 		inScope,
											AudioUnitElement 	inElement,
											const void *		inData,
											UInt32              inDataSize)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			
			case kAudioUnitCustomProperty_BRRData:
			{
				BRRData *brr = (BRRData *)inData;
				mEfx->SetBRRData(brr);
				return noErr;
			}

			case kAudioUnitCustomProperty_Rate:
				mEfx->SetPropertyValue(inID, *((double*)inData));
				return noErr;
				
			case kAudioUnitCustomProperty_BaseKey:
			case kAudioUnitCustomProperty_LowKey:
			case kAudioUnitCustomProperty_HighKey:
			case kAudioUnitCustomProperty_LoopPoint:
			case kAudioUnitCustomProperty_Bank:
			case kAudioUnitCustomProperty_AR:
			case kAudioUnitCustomProperty_DR:
			case kAudioUnitCustomProperty_SL:
			case kAudioUnitCustomProperty_SR:
			case kAudioUnitCustomProperty_VolL:
			case kAudioUnitCustomProperty_VolR:
			case kAudioUnitCustomProperty_EditingProgram:
			case kAudioUnitCustomProperty_EditingChannel:
			case kAudioUnitCustomProperty_MaxNoteTrack_1:
			case kAudioUnitCustomProperty_MaxNoteTrack_2:
			case kAudioUnitCustomProperty_MaxNoteTrack_3:
			case kAudioUnitCustomProperty_MaxNoteTrack_4:
			case kAudioUnitCustomProperty_MaxNoteTrack_5:
			case kAudioUnitCustomProperty_MaxNoteTrack_6:
			case kAudioUnitCustomProperty_MaxNoteTrack_7:
			case kAudioUnitCustomProperty_MaxNoteTrack_8:
			case kAudioUnitCustomProperty_MaxNoteTrack_9:
			case kAudioUnitCustomProperty_MaxNoteTrack_10:
			case kAudioUnitCustomProperty_MaxNoteTrack_11:
			case kAudioUnitCustomProperty_MaxNoteTrack_12:
			case kAudioUnitCustomProperty_MaxNoteTrack_13:
			case kAudioUnitCustomProperty_MaxNoteTrack_14:
			case kAudioUnitCustomProperty_MaxNoteTrack_15:
			case kAudioUnitCustomProperty_MaxNoteTrack_16:
				mEfx->SetPropertyValue(inID, *((int*)inData) );
				return noErr;
				
			case kAudioUnitCustomProperty_Loop:
			case kAudioUnitCustomProperty_Echo:
			case kAudioUnitCustomProperty_IsEmaphasized:
				mEfx->SetPropertyValue(inID, *((bool*)inData) ? 1.0f:.0f );
				return noErr;
				
			case kAudioUnitCustomProperty_TotalRAM:
				return noErr;
				
			case kAudioUnitCustomProperty_PGDictionary:
			{
				CFDictionaryRef	pgdata = *((CFDictionaryRef*)inData);
				int				editProg = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingProgram);
				RestorePGDataDic(pgdata, editProg);
				return noErr;
			}
				
			case kAudioUnitCustomProperty_XIData:
			{
				return noErr;
			}
			
			case kAudioUnitCustomProperty_ProgramName:
			{
				char	pgname[PROGRAMNAME_MAX_LEN];
				CFStringGetCString(*((CFStringRef*)inData), pgname, PROGRAMNAME_MAX_LEN-1, kCFStringEncodingUTF8);
				mEfx->SetProgramName(pgname);
				return noErr;
			}
				
			//エコー
			case kAudioUnitCustomProperty_Band1:
			case kAudioUnitCustomProperty_Band2:
			case kAudioUnitCustomProperty_Band3:
			case kAudioUnitCustomProperty_Band4:
			case kAudioUnitCustomProperty_Band5:
				mEfx->SetPropertyValue(inID, *((float*)inData) );
				return noErr;
				
			case kAudioUnitCustomProperty_NoteOnTrack_1:
			case kAudioUnitCustomProperty_NoteOnTrack_2:
			case kAudioUnitCustomProperty_NoteOnTrack_3:
			case kAudioUnitCustomProperty_NoteOnTrack_4:
			case kAudioUnitCustomProperty_NoteOnTrack_5:
			case kAudioUnitCustomProperty_NoteOnTrack_6:
			case kAudioUnitCustomProperty_NoteOnTrack_7:
			case kAudioUnitCustomProperty_NoteOnTrack_8:
			case kAudioUnitCustomProperty_NoteOnTrack_9:
			case kAudioUnitCustomProperty_NoteOnTrack_10:
			case kAudioUnitCustomProperty_NoteOnTrack_11:
			case kAudioUnitCustomProperty_NoteOnTrack_12:
			case kAudioUnitCustomProperty_NoteOnTrack_13:
			case kAudioUnitCustomProperty_NoteOnTrack_14:
			case kAudioUnitCustomProperty_NoteOnTrack_15:
			case kAudioUnitCustomProperty_NoteOnTrack_16:
				return noErr;
				
			case kAudioUnitCustomProperty_SourceFileRef:
			{
				CFStringRef pathStr = CFURLCopyFileSystemPath(*((CFURLRef*)inData), kCFURLPOSIXPathStyle);
				char		path[PATH_LEN_MAX];
				CFStringGetCString(pathStr, path, PATH_LEN_MAX-1, kCFStringEncodingUTF8);
				CFRelease(pathStr);
				mEfx->SetSourceFilePath(path);
				return noErr;
			}
		}
	}
	return AUInstrumentBase::SetProperty(inID, inScope, inElement, inData, inDataSize);
}

//-----------------------------------------------------------------------------
//	Chip700::GetPresets
//-----------------------------------------------------------------------------
ComponentResult Chip700::GetPresets(CFArrayRef *outData) const
{	
	if (outData == NULL) return noErr;
	
	CFMutableArrayRef theArray = CFArrayCreateMutable(NULL, NUM_PRESETS, NULL);
	for (int i = 0; i < NUM_PRESETS; ++i) {
		CFArrayAppendValue(theArray, &mPresets[i]);
    }
    
	*outData = (CFArrayRef)theArray;
	return noErr;
}

//-----------------------------------------------------------------------------
OSStatus Chip700::NewFactoryPresetSet(const AUPreset &inNewFactoryPreset)
{
	UInt32 chosenPreset = inNewFactoryPreset.presetNumber;
	if ( chosenPreset < (UInt32)NUM_PRESETS ) {
		mEfx->SelectPreset(chosenPreset);
		char cname[32];
		CFStringGetCString(inNewFactoryPreset.presetName, cname, 32, kCFStringEncodingASCII);
		//mEfx->setProgramName(cname);
		SetAFactoryPresetAsCurrent(inNewFactoryPreset);
		for ( UInt32 i=0; i<kNumberOfParameters; i++ ) {
			Globals()->SetParameter(i, mEfx->GetParameter(i));
		}
		return noErr;
	}
	return kAudioUnitErr_InvalidPropertyValue;
}


//-----------------------------------------------------------------------------
static void AddNumToDictionary(CFMutableDictionaryRef dict, CFStringRef key, int value)
{
	CFNumberRef num = CFNumberCreate(NULL, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, key, num);
	CFRelease(num);
}

//-----------------------------------------------------------------------------
static void AddBooleanToDictionary(CFMutableDictionaryRef dict, CFStringRef key, bool value)
{
	if ( value ) {
		CFDictionarySetValue(dict, key, kCFBooleanTrue);
	}
	else {
		CFDictionarySetValue(dict, key, kCFBooleanFalse);
	}
}

//-----------------------------------------------------------------------------
//	Chip700::SaveState
//-----------------------------------------------------------------------------
ComponentResult	Chip700::SaveState(CFPropertyListRef *outData)
{
	ComponentResult result;
	result = AUInstrumentBase::SaveState(outData);
	CFMutableDictionaryRef	dict=(CFMutableDictionaryRef)*outData;
	if (result == noErr) {
		CFDictionaryRef	pgdata;
		CFStringRef pgnum,pgname;
		
		
		for (int i=0; i<128; i++) {
			const BRRData		*brr = mEfx->GetBRRData(i);
			if (brr->data) {
				CreatePGDataDic(&pgdata, i);
				pgnum = CFStringCreateWithFormat(NULL,NULL,CFSTR("pg%03d"),i);
				CFDictionarySetValue(dict, pgnum, pgdata);
				CFRelease(pgdata);
				CFRelease(pgnum);
			}
		}
		
		// 作業状態を保存
		int	editProg = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingProgram);
		int	editChan = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingChannel);
		AddNumToDictionary(dict, kSaveKey_EditProg, editProg);
		AddNumToDictionary(dict, kSaveKey_EditChan, editChan);
		
		pgname = CFStringCreateCopy(NULL,CFSTR("C700"));
		CFDictionarySetValue(dict, CFSTR(kAUPresetNameKey), pgname);
		CFRelease(pgname);
	}
	return result;
}

//-----------------------------------------------------------------------------
//	Chip700::RestoreState
//-----------------------------------------------------------------------------
ComponentResult	Chip700::RestoreState(CFPropertyListRef plist)
{
	ComponentResult result;
	result = AUInstrumentBase::RestoreState(plist);
	CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);
	if (result == noErr) {
		//波形情報の復元
		CFStringRef pgnum;
		CFDictionaryRef	pgdata;
		for (int i=0; i<128; i++) {
			pgnum = CFStringCreateWithFormat(NULL,NULL,CFSTR("pg%03d"),i);
			if (CFDictionaryContainsKey(dict, pgnum)) {
				pgdata = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, pgnum));
				RestorePGDataDic(pgdata, i);
			}
			CFRelease(pgnum);
		}
		
		if (CFDictionaryContainsKey(dict, kSaveKey_EditProg)) {
			int	editProg;
			CFNumberRef cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_EditProg));
			CFNumberGetValue(cfnum, kCFNumberIntType, &editProg);
			
			//Globals()->SetParameter(kParam_program, editProg);
			//変更の通知
			//PropertyChanged(kAudioUnitCustomProperty_EditingProgram, kAudioUnitScope_Global, 0);
			mEfx->SetPropertyValue(kAudioUnitCustomProperty_EditingProgram, editProg);
		}
		if (CFDictionaryContainsKey(dict, kSaveKey_EditChan)) {
			int	editChan = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingChannel);
			CFNumberRef cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_EditChan));
			CFNumberGetValue(cfnum, kCFNumberIntType, &editChan);
			
			//変更の通知
			//PropertyChanged(kAudioUnitCustomProperty_EditingChannel, kAudioUnitScope_Global, 0);
			mEfx->SetPropertyValue(kAudioUnitCustomProperty_EditingChannel, editChan);
		}
	}
	return result;
}

//-----------------------------------------------------------------------------
int Chip700::CreatePGDataDic(CFDictionaryRef *data, int pgnum)
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable	(NULL, 0, 
								&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	const VoiceParams	*vpSet = mEfx->GetVP();
	
	if (vpSet[pgnum].loop) {
		vpSet[pgnum].brr.data[vpSet[pgnum].brr.size - 9] |= 2;
	}
	else {
		vpSet[pgnum].brr.data[vpSet[pgnum].brr.size - 9] &= ~2;
	}
	CFDataRef	brrdata = CFDataCreate(NULL, vpSet[pgnum].brr.data, vpSet[pgnum].brr.size);
	CFDictionarySetValue(dict, kSaveKey_brrdata, brrdata);
	CFRelease(brrdata);
	
	AddNumToDictionary(dict, kSaveKey_looppoint, vpSet[pgnum].lp);
	CFNumberRef	num = CFNumberCreate(NULL, kCFNumberDoubleType, &vpSet[pgnum].rate);
	CFDictionarySetValue(dict, kSaveKey_samplerate, num);
	CFRelease(num);
	AddNumToDictionary(dict, kSaveKey_basekey, vpSet[pgnum].basekey);
	AddNumToDictionary(dict, kSaveKey_lowkey, vpSet[pgnum].lowkey);
	AddNumToDictionary(dict, kSaveKey_highkey, vpSet[pgnum].highkey);
	AddNumToDictionary(dict, kSaveKey_ar, vpSet[pgnum].ar);
	AddNumToDictionary(dict, kSaveKey_dr, vpSet[pgnum].dr);
	AddNumToDictionary(dict, kSaveKey_sl, vpSet[pgnum].sl);
	AddNumToDictionary(dict, kSaveKey_sr, vpSet[pgnum].sr);
	AddNumToDictionary(dict, kSaveKey_volL, vpSet[pgnum].volL);
	AddNumToDictionary(dict, kSaveKey_volR, vpSet[pgnum].volR);
	AddBooleanToDictionary(dict, kSaveKey_echo, vpSet[pgnum].echo);
	AddNumToDictionary(dict, kSaveKey_bank, vpSet[pgnum].bank);
	
	//元波形情報
	AddBooleanToDictionary(dict, kSaveKey_IsEmphasized, vpSet[pgnum].isEmphasized);
	if ( vpSet[pgnum].sourceFile[0] ) {
		CFURLRef	url = 
		CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*)vpSet[pgnum].sourceFile, 
												strlen(vpSet[pgnum].sourceFile), false);
		CFDataRef urlData = CFURLCreateData( NULL, url, kCFStringEncodingUTF8, false );
		CFDictionarySetValue(dict, kSaveKey_SourceFile, urlData);
		CFRelease(urlData);
		CFRelease(url);
	}
	
	//プログラム名
	if (vpSet[pgnum].pgname[0] != 0) {
		CFStringRef	str = CFStringCreateWithCString(NULL, vpSet[pgnum].pgname, kCFStringEncodingUTF8);
		CFDictionarySetValue(dict, kSaveKey_ProgName, str);
		CFRelease(str);
	}
	
	*data = dict;
	return 0;
}

//-----------------------------------------------------------------------------
void Chip700::RestorePGDataDic(CFPropertyListRef data, int pgnum)
{
	int editProg = mEfx->GetPropertyValue(kAudioUnitCustomProperty_EditingProgram);
	mEfx->SetPropertyValue(kAudioUnitCustomProperty_EditingProgram, pgnum);

	CFDictionaryRef dict = static_cast<CFDictionaryRef>(data);
	CFNumberRef cfnum;
	
	CFDataRef cfdata = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(dict, kSaveKey_brrdata));
	int	size = CFDataGetLength(cfdata);
	const UInt8	*dataptr = CFDataGetBytePtr(cfdata);
	BRRData		brr;
	brr.data = const_cast<unsigned char*>(dataptr);
	brr.size = size;
	mEfx->SetBRRData(&brr);
//	mVPset[pgnum].brr.data = new unsigned char[size];
//	memmove(mVPset[pgnum].brr.data,dataptr,size);
//	mVPset[pgnum].brr.size = size;
	mEfx->SetPropertyValue(kAudioUnitCustomProperty_Loop, 
						   brr.data[brr.size-9]&2?true:false);
//	mVPset[pgnum].loop = brr.data[brr.size-9]&2?true:false;
	
	int	value;
	if (CFDictionaryContainsKey(dict, kSaveKey_looppoint)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_looppoint));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].lp = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_LoopPoint, value);
	}
	
	double	dvalue;
	if (CFDictionaryContainsKey(dict, kSaveKey_samplerate)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_samplerate));
		CFNumberGetValue(cfnum, kCFNumberDoubleType, &dvalue);
		//mVPset[pgnum].rate = dvalue;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_Rate, dvalue);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_basekey)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_basekey));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].basekey = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_BaseKey, value);
	}
	else {
		//mVPset[pgnum].basekey = 60;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_BaseKey, 60);
	}
	if (CFDictionaryContainsKey(dict, kSaveKey_lowkey)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_lowkey));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].lowkey = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_LowKey, value);
	}
	else {
		//mVPset[pgnum].lowkey = 0;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_LowKey, 0);
	}
	if (CFDictionaryContainsKey(dict, kSaveKey_highkey)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_highkey));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].highkey = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_HighKey,value );
	}
	else {
		//mVPset[pgnum].highkey = 127;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_HighKey,127 );
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_ar)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_ar));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].ar = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_AR, value);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_dr)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_dr));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].dr = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_DR, value);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_sl)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_sl));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].sl = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_SL, value);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_sr)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_sr));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].sr = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_SR, value);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_volL)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_volL));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].volL = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_VolL, value);
	}
	else {
		//mVPset[pgnum].volL = 100;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_VolL, 100);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_volR)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_volR));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].volR = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_VolR, value);
	}
	else {
		//mVPset[pgnum].volR = 100;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_VolR, 100);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_echo)) {
		CFBooleanRef cfbool = reinterpret_cast<CFBooleanRef>(CFDictionaryGetValue(dict, kSaveKey_echo));
		//mVPset[pgnum].echo = CFBooleanGetValue(cfbool) ? true:false;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_Echo,CFBooleanGetValue(cfbool) ? 1.0f:.0f);
	}
	else {
		//mVPset[pgnum].echo = false;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_Echo, .0f);
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_bank)) {
		cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_bank));
		CFNumberGetValue(cfnum, kCFNumberIntType, &value);
		//mVPset[pgnum].bank = value;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_Bank, value );
	}
	else {
		//mVPset[pgnum].bank = 0;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_Bank, 0 );
	}
	
	if (CFDictionaryContainsKey(dict, kSaveKey_ProgName)) {
		char	pgname[PROGRAMNAME_MAX_LEN];
		CFStringGetCString(reinterpret_cast<CFStringRef>(CFDictionaryGetValue(dict, kSaveKey_ProgName)),
						   pgname, PROGRAMNAME_MAX_LEN, kCFStringEncodingUTF8);
		mEfx->SetProgramName(pgname);
	}
	else {
		//mVPset[pgnum].pgname[0] = 0;
		mEfx->SetProgramName("");
	}
	
	//元波形ファイル情報を復元
	if (CFDictionaryContainsKey(dict, kSaveKey_SourceFile)) {
		CFDataRef	urlData = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(dict, kSaveKey_SourceFile));
		CFURLRef	url = CFURLCreateWithBytes( NULL, CFDataGetBytePtr(urlData), 
											   CFDataGetLength(urlData), kCFStringEncodingUTF8, NULL );
		CFStringRef pathStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
		char	path[PATH_LEN_MAX];
		CFStringGetCString(pathStr, path, PATH_LEN_MAX-1, kCFStringEncodingUTF8);
		mEfx->SetSourceFilePath(path);
		CFRelease(pathStr);
		CFRelease(url);
		
		CFBooleanRef cfbool = reinterpret_cast<CFBooleanRef>(CFDictionaryGetValue(dict, kSaveKey_IsEmphasized));
		//mVPset[pgnum].isEmphasized = CFBooleanGetValue(cfbool) ? true:false;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_IsEmaphasized, CFBooleanGetValue(cfbool) ? 1.0f:.0f);
	}
	else {
		//mVPset[pgnum].sourceFile[0] = 0;
		mEfx->SetSourceFilePath("");
		//mVPset[pgnum].isEmphasized = true;
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_IsEmaphasized, .0f);
	}
	
	//UIに変更を反映
	if (pgnum == editProg) {
		//for (int i=kAudioUnitCustomProperty_ProgramName; i<=kAudioUnitCustomProperty_Bank; i++) {
		//	PropertyChanged(i, kAudioUnitScope_Global, 0);
		//}
		mEfx->SetPropertyValue(kAudioUnitCustomProperty_EditingProgram, editProg);
	}
	
	mEfx->GetGenerator()->RefreshKeyMap();
}
//-----------------------------------------------------------------------------
ComponentResult Chip700::RealTimeStartNote(	SynthGroupElement 			*inGroup,
											NoteInstanceID 				inNoteInstanceID, 
											UInt32 						inOffsetSampleFrame, 
											const MusicDeviceNoteParams &inParams)
{
	//MIDIチャンネルの取得
	unsigned int		chID = inGroup->GroupID() % 16;
	
	mEfx->HandleNoteOn(chID, inParams.mPitch, inParams.mVelocity, inNoteInstanceID+chID*256, inOffsetSampleFrame);
	return noErr;
}

//-----------------------------------------------------------------------------
ComponentResult Chip700::RealTimeStopNote( MusicDeviceGroupID 			inGroup, 
										   NoteInstanceID 				inNoteInstanceID, 
										   UInt32 						inOffsetSampleFrame)
{
	AUInstrumentBase::RealTimeStopNote( inGroup, inNoteInstanceID, inOffsetSampleFrame );
	//MIDIチャンネルの取得
	unsigned int		chID = inGroup % 16;
	
	mEfx->HandleNoteOff(chID, 0xff, inNoteInstanceID+chID*256, inOffsetSampleFrame);
	return noErr;
}

//-----------------------------------------------------------------------------
OSStatus Chip700::HandlePitchWheel(	UInt8 	inChannel,
							   UInt8 	inPitch1,
							   UInt8 	inPitch2,
							   UInt32	inStartFrame)
{
	mEfx->HandlePitchBend(inChannel, inPitch1, inPitch2, inStartFrame);
	return noErr;
}

//-----------------------------------------------------------------------------
//	Chip700::HandleControlChange
//-----------------------------------------------------------------------------
OSStatus Chip700::HandleControlChange(	UInt8 	inChannel,
									UInt8 	inController,
									UInt8 	inValue,
									UInt32	inStartFrame)
{
	mEfx->HandleControlChange(inChannel, inController, inValue, inStartFrame);	//モジュレーションホイール
	return AUInstrumentBase::HandleControlChange(inChannel, inController, inValue, inStartFrame);
}

//-----------------------------------------------------------------------------
//	Chip700::HandleProgramChange
//-----------------------------------------------------------------------------
OSStatus Chip700::HandleProgramChange(	UInt8	inChannel,
										UInt8	inValue)
{
	mEfx->HandleProgramChange(inChannel, inValue, 0);
	return AUInstrumentBase::HandleProgramChange(inChannel, inValue);
}

//-----------------------------------------------------------------------------
OSStatus Chip700::HandleResetAllControllers( UInt8 	inChannel)
{
	mEfx->HandleResetAllControllers(inChannel, 0);
	return AUInstrumentBase::HandleResetAllControllers( inChannel);
}

//-----------------------------------------------------------------------------
OSStatus Chip700::HandleAllNotesOff( UInt8 inChannel )
{
	mEfx->HandleAllNotesOff(inChannel, 0);
	return AUInstrumentBase::HandleAllNotesOff( inChannel);
}

//-----------------------------------------------------------------------------
OSStatus Chip700::HandleAllSoundOff( UInt8 inChannel )
{
	mEfx->HandleAllSoundOff(inChannel, 0);
	return AUInstrumentBase::HandleAllSoundOff( inChannel);
}
