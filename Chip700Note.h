/*
 *  Chip700Note.h
 *  Chip700
 *
 *  Created by 開発用 on 06/09/06.
 *  Copyright 2006 Vermicelli Magic. All rights reserved.
 *
 */

#ifndef __Chip700Note_h__
#define __Chip700Note_h__

#include "AUInstrumentBase.h"
#include "Chip700Version.h"
#include "Chip700defines.h"
#include <list>

typedef enum
{
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
	FASTRELEASE
} env_state_t32;

class EchoKernel
{
public:
	EchoKernel()
	{
		mDelayUnit=512;
		mEchoBuffer.AllocateClear(7680);
		mFIRLength=8;
		mFIRbuf.AllocateClear(mFIRLength);
		mFilterStride=1;
		mEchoIndex=0;
		mFIRIndex=0;
		
		m_echo_vol=0;
		m_input = 0;
	};
	
	void 	Input(int samp);
	int		GetFxOut();
	void	Reset();
	void	SetEchoVol( int val )
	{
		m_echo_vol = val;
	}
	void	SetFBLevel( int val )
	{
		m_fb_lev = val;
	}
	void	SetFIRTap( int index, int val )
	{
		m_fir_taps[index] = val;
	}
	void	SetDelayTime( int val )
	{
		m_delay_samples = val * mDelayUnit;
	}
	
private:
	TAUBuffer<int>	mEchoBuffer;
	TAUBuffer<int>	mFIRbuf;
	int			mEchoIndex;
	int			mFIRIndex, mFIRLength;
	int			mDelayUnit;
	int			mFilterStride;
	
	int			m_echo_vol;
	int			m_fb_lev;
	int			m_fir_taps[8];
	int			m_delay_samples;
	
	int			m_input;
};

struct Chip700Note : public SynthNote
{
public:
	Chip700Note();
	virtual				~Chip700Note() {}
	virtual void		Reset();
	
	virtual void		Attack(const MusicDeviceNoteParams &inParams);
	virtual void		Kill(UInt32 inFrame);
	virtual void		Release(UInt32 inFrame);
	virtual void		FastRelease(UInt32 inFrame);
	virtual Float32		Amplitude();
	virtual OSStatus	Render(UInt32 inNumFrames, AudioBufferList& inBufferList);
	
	void KeyOn( unsigned char ch, unsigned char note, unsigned char velo, int inFrame );
	void KeyOff( unsigned char ch, unsigned char note, unsigned char velo, int inFrame );
	
	void ProgramChange( int ch, int pgnum, int inFrame );
	void PitchBend( int ch, int value, int inFrame );
	void ModWheel( int ch, int value, int inFrame );
	void Damper( int ch, int value, int inFrame );
	
	void SetPBRange( float value );
	void SetClipper( int value );
	void SetVibFreq( float value );
	void SetVibDepth( float value );
	
	void SetMainVol_L( int value );
	void SetMainVol_R( int value );
	void SetEchoVol_L( int value );
	void SetEchoVol_R( int value );
	void SetFeedBackLevel( int value );
	void SetDelayTime( int value );
	void SetFIRTap( int tap, int value );
	
	void SetSampleRate( double samplerate ) { mSampleRate = samplerate; }
	
	void Process( unsigned int frames, float *output[2] );
	int *GetKeyMap() { return mKeyMap; }
	VoiceParams getVP(int pg) {return mVPset[pg];};
	VoiceParams getMappedVP(int key) {return mVPset[mKeyMap[key]];};
	void SetVPSet( VoiceParams *vp ) { mVPset = vp; }
	
	void RefreshKeyMap(void);
	
private:
	static const int INTERNAL_CLOCK = 32000;
	static const int MAX_VOICES = 16;
	enum EvtType {
		NOTE_ON = 0,
		NOTE_OFF
	};
	
	typedef struct {
		unsigned char	type;
		unsigned char	ch;
		unsigned char	note;
		unsigned char	velo;
		int		remain_samples;
	} NoteEvt;
	
	struct VoiceState {
		int				midi_ch;
		
		unsigned char	*brrdata;
		int				memPtr;        /* Sample data memory pointer   */
		int             end;            /* End or loop after block      */
		int             envcnt;         /* Counts to envelope update    */
		env_state_t32   envstate;       /* Current envelope state       */
		int             envx;           /* Last env height (0-0x7FFF)   */
		int             filter;         /* Last header's filter         */
		int             half;           /* Active nybble of BRR         */
		int             headerCnt;     /* Bytes before new header (0-8)*/
		int             mixfrac;        /* Fractional part of smpl pstn */	//サンプル間を4096分割した位置
		int				pitch;          /* Sample pitch (4096->32000Hz) */
		int             range;          /* Last header's range          */
		int             sampptr;        /* Where in sampbuf we are      */
		int				smp1;           /* Last sample (for BRR filter) */
		int				smp2;           /* Second-to-last sample decoded*/
		int				sampbuf[4];   /* Buffer for Gaussian interp   */
		
		
		int				pb;
		int				vibdepth;
		bool			reg_pmod;
		float			vibPhase;
		
		int				ar,dr,sl,sr,vol_l,vol_r;
		
		int				velo;
		unsigned int	loopPoint;
		bool			loop;
	
		bool			echoOn;
		
		void Reset();
	};
	
	double			mSampleRate;
	
	int				mProcessFrac;
	int				mProcessbuf[2][16];		//リサンプリング用バッファ
	int				mProcessbufPtr;			//リサンプリング用バッファ書き込み位置
	EchoKernel		mEcho[2];
	
	std::list<NoteEvt>	mNoteEvt;			//受け取ったイベントのキュー
	
	VoiceState		mVoice[MAX_VOICES];		//ボイスの状況
	
	int				mMainVolume_L;
	int				mMainVolume_R;
	float			mVibfreq;
	int				mVibdepth;
	float			mPbrange;
	int				mClipper;
	int				mChProgram[16];
	float			mChPitchBend[16];
	int				mChVibDepth[16];
	
	int				mKeyMap[128];	//各キーに対応するプログラムNo.
	VoiceParams		*mVPset;

	int		_note;
	
	int FindVoice( const NoteEvt *evt );
	void DoKeyOn(NoteEvt *evt);
	float VibratoWave(float phase);
	void CalcPBValue(float pitchBend, int basePitch);
};


#endif
