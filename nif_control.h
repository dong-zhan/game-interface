#ifndef _INCLUDE_NIF_CONTROL_H_
#define _INCLUDE_NIF_CONTROL_H_

#include "ge_util/hashlist.h"
#include "app11/app11fontbitmap.h"
#include "app11/app11dynamicbuffer.h"

#include "sp/sp_dynbvh.h"
#include "ge_util/poollist.h"

#include "nif_base.h"


/////////////////////////////////////////////////////////////////////////////////
//				CNIFRadioControl
//
class CNIFRadioControl
{
protected:
	unsigned int radio_control_id;
	unsigned int text_control_id;

	CNIFStaticBuffer* sb;

	unsigned int alignId;
	unsigned char slice;
	int imageW, iconW;
	int iconX, iconY, iconPressedX, iconPressedY;
	int gap;		//gap from radio to text

	bool pressed;

public:
	void init(CNIFStaticBuffer* sb, unsigned int radio_control_id, unsigned int text_control_id, int gap, 
		unsigned int alignId, unsigned char slice,
		int imageW, int iconW, int iconX, int iconY, int iconPressedX, int iconPressedY);

	void set(int x0, int x1, int y, wchar_t* str, unsigned char* colors);
	void del(void);
	void move(int dx, int dy);
	void onClick(void);

public:
	bool isPressed(void) { return pressed; }
};


/////////////////////////////////////////////////////////////////////////////////
//				CNIFScrollBar
//
class CNIFScrollBar
{
protected:
	unsigned int up_control_id;
	unsigned int down_control_id;
	unsigned int slider_control_id;

	CNIFStaticBuffer* sb;

	unsigned int alignId;
	unsigned char slice;
	int imageW, iconW;

	int upX, upY;
	int downX, downY;
	int sliderX, sliderY;

	int x0, x1, y0, y1;

	bool bHorizontal;

public:
	void init(CNIFStaticBuffer* sb, unsigned int up_control_id, unsigned int down_control_id, unsigned int slider_control_id,
		int imageW, int iconW, unsigned char slice, unsigned int alignId,
		int upX, int upY, int downX, int downY, int SliderX, int sliderY,
		int x0, int x1, int y0, int y1,
		bool bHorizontal
	);

	void onSliderMove(int dx, int dy);

public:
	int getScrollBarRange(void);
	int getSliderPos(void);
};


/////////////////////////////////////////////////////////////////////////////////
//				CNIFListBox
//
class CNIFListBox
{
protected:
	CGrowableArrayWithLast<unsigned int>ids;

	CNIFStaticBuffer* sb;

	unsigned int alignId;

	int x0, x1, y0, y1;		//the entire area for all textboxes
	int xMargin, yMargin, gap;

public:
	void init(CNIFStaticBuffer* sb, unsigned int* ids, unsigned int cnt, 
		int x0, int x1, int y0, int y1, 
		int gap);

	void setText(unsigned int idx, wchar_t* str, unsigned char* colors);
};

/////////////////////////////////////////////////////////////////////////////////
//				CNIFComboBox
//
class CNIFComboBox
{
protected:
	CNIFListBox listBox;

	unsigned int textBoxId, dropDownButtonId;

public:

};

#endif
