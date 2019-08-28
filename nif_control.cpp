#include "stdafx.h"
#include "ge_util/allocators.h"
#include "app11/app11fontbitmap.h"
#include "math/mathlib.h"

#include "nif_control.h"

#include <tchar.h>

#include "app11/app11tech.h"

extern CApp11AllTechs allTechs;

void CNIFRadioControl::init(CNIFStaticBuffer* sb, unsigned int radio_control_id, unsigned int text_control_id,
	int gap, unsigned int alignId, unsigned char slice,
	int imageW, int iconW, int iconX, int iconY, int iconPressedX, int iconPressedY)
{
	this->sb = sb;

	this->radio_control_id = radio_control_id;
	this->text_control_id = text_control_id;

	this->alignId = alignId;
	this->gap = gap;
	this->slice = slice;
	this->imageW = imageW;
	this->iconW = iconW;
	this->iconX = iconX;
	this->iconY = iconY;
	this->iconPressedX = iconPressedX;
	this->iconPressedY = iconPressedY;

	pressed = false;
}

void CNIFRadioControl::set(int x0, int x1, int y, wchar_t* str, unsigned char* colors)
{
	int h = sb->getTex().getDynBitmap()->get_cell_height();

	//
	// add radio
	//
	sb->addSquardIcon(radio_control_id, alignId, slice, imageW, iconW, iconX, iconY, x0, y, true);

	int strY;
	
	if (iconW > h) {
		strY = (iconW - h) >> 1;
		strY += y;
	}
	else {
		strY = y;
	}
	//
	// add text
	//
	sb->addText(text_control_id, alignId, str, colors, x0 + iconW + gap, x1, strY, strY + h, true);
}

void CNIFRadioControl::del(void)
{
}

void CNIFRadioControl::move(int dx, int dy)
{
	sb->moveControl(radio_control_id, dx, dy);
	sb->moveControl(text_control_id, dx, dy);
}

void CNIFRadioControl::onClick(void)
{
	//
	// find control
	//
	CNIFStaticBuffer::ControlInfo* ci = sb->avlFind(radio_control_id);
	geAssert(ci);

	//
	// change to opposite status
	//
	pressed = !pressed;
	int ix, iy;
	if (pressed) {
		ix = iconPressedX;
		iy = iconPressedY;
	}
	else {
		ix = iconX;
		iy = iconY;
	}

	//
	// update uv
	//
	float iw = (float)iconW / (float)imageW;
	float fx0 = ix * iw;
	float fy0 = iy * iw;
	float fx1 = (ix + 1) * iw;
	float fy1 = (iy + 1) * iw;

	CApp11DynamicBuffer<CVector2>& vbUv = sb->getVbUv();

	CVector2* uv = vbUv.getData().get_objects() + ci->vbPosIdx * 4;
	uv[0].set(fx0, fy0);
	uv[1].set(fx1, fy0);
	uv[2].set(fx0, fy1);
	uv[3].set(fx1, fy1);

	ci->ltu = fx0;
	ci->ltv = fy0;
	ci->bru = fx1;
	ci->brv = fy1;

	//should not update here, because this may trigger other operations.
	//vbUv.updateBuffer();
}

/////////////////////////////////////////////////////////////////////////////////
//				CNIFScrollBar
//
void CNIFScrollBar::init(CNIFStaticBuffer* sb, unsigned int up_control_id, unsigned int down_control_id, unsigned int slider_control_id, 
	int imageW, int iconW, unsigned char slice, unsigned int alignId, 
	int upX, int upY, int downX, int downY, int sliderX, int sliderY,
	int x0, int x1, int y0, int y1, bool bHorizontal)
{
	this->sb = sb;
	this->up_control_id = up_control_id;
	this->down_control_id = down_control_id;
	this->slider_control_id = slider_control_id;
	this->imageW = imageW;
	this->iconW = iconW;
	this->slice = slice;
	this->alignId = alignId;
	this->upX = upX;
	this->upY = upY;
	this->downX = downX;
	this->downY = downY;
	this->sliderX = sliderX;
	this->sliderY = sliderY;
	this->x0 = x0;
	this->x1 = x1;
	this->y0 = y0;
	this->y1 = y1;
	this->bHorizontal = bHorizontal;

	if (bHorizontal) {
		sb->addSquardIcon(up_control_id, alignId, slice, imageW, iconW, upX, upY, x0, y0, true);
		sb->addSquardIcon(down_control_id, alignId, slice, imageW, iconW, downX, downY, x1 - iconW, y0, true);
		sb->addSquardIcon(slider_control_id, alignId, slice, imageW, iconW, sliderX, sliderY, x1- iconW - iconW, y0, true);
	}
	else {
		sb->addSquardIcon(up_control_id, alignId, slice, imageW, iconW, upX, upY, x0, y0, true);
		sb->addSquardIcon(down_control_id, alignId, slice, imageW, iconW, downX, downY, x0, y1 - iconW, true);
		sb->addSquardIcon(slider_control_id, alignId, slice, imageW, iconW, sliderX, sliderY, x0, y1 - iconW - iconW, true);
	}
}

void CNIFScrollBar::onSliderMove(int dx, int dy)
{
	//
	// find control
	//
	CNIFStaticBuffer::ControlInfo* ci = sb->avlFind(slider_control_id);
	geAssert(ci);

	//
	// del in bvh
	//
	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)ci->x0;
	box.vmax.x = (float)ci->x1;
	box.vmin.z = (float)ci->y0;
	box.vmax.z = (float)ci->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bool b = sb->bvhDel(box, slider_control_id);
	geAssert(b);

	//
	// transform x0,x1,y0,y1 to resized window space
	//
	CNIFBase::Orig& orig = sb->getAlignOrigs()[alignId];

	int tx0 = x0 + orig.x;
	int tx1 = x1 + orig.x;
	int ty0 = y0 + orig.y;
	int ty1 = y1 + orig.y;

	//
	// update pos
	//
	if (bHorizontal) {
		int txmin = tx0 + iconW;
		int txmax = tx1 - iconW;
		tx0 = ci->x0;
		tx1 = ci->x1;
		tx0 += dx;
		tx1 += dx;
		if (tx0 < txmin) {
			tx0 = txmin;
			tx1 = txmin + iconW;
		}
		if (tx1 > txmax) {
			tx0 = txmax - iconW;
			tx1 = txmax;
		}
	}
	else {
		int tymin = ty0 + iconW;
		int tymax = ty1 - iconW;
		ty0 = ci->y0;
		ty1 = ci->y1;
		ty0 += dy;
		ty1 += dy;
		if (ty0 < tymin) {
			ty0 = tymin;
			ty1 = tymin + iconW;
		}
		if (ty1 > tymax) {
			ty0 = tymax - iconW;
			ty1 = tymax;
		}
	}

	ci->x0 = tx0;
	ci->x1 = tx1;
	ci->y0 = ty0;
	ci->y1 = ty1;

	//
	// update bvh
	//
	box.vmin.x = (float)ci->x0;
	box.vmax.x = (float)ci->x1;
	box.vmin.z = (float)ci->y0;
	box.vmax.z = (float)ci->y1;

	CNIFStaticBuffer::bvh_type::node_pointer_type node = sb->bvhAdd(box);
	node->t = slider_control_id;

	//
	//transform coordinates from lefttop(0,0) to center(0,0)
	//
	tx0 -= sb->getScreenW() >> 1;			//from left(0) to center(0)
	tx1 -= sb->getScreenW() >> 1;
	ty0 = sb->getScreenH() - ty0;			//from bottom(0) to top(0), axis goes top-down
	ty1 = sb->getScreenH() - ty1;
	ty0 -= sb->getScreenH() >> 1;			//from top(0) to center(0), axis goes top-down
	ty1 -= sb->getScreenH() >> 1;

	//
	//
	//
	float pixelHeight = 2.f / sb->getScreenH();
	float pixelWidth = 2.f / sb->getScreenW();

	float yTop = ty0 * pixelHeight;
	float yBot = ty1 * pixelHeight;
	float xa = tx0 * pixelWidth;
	float xb = tx1 * pixelWidth;

	yTop -= pixelHeight * 0.5f;		//TODO: is this shifting really still needed??? (microsoft map texel to pixel)
	yBot -= pixelHeight * 0.5f;
	xa -= pixelWidth * 0.5f;
	xb -= pixelWidth * 0.5f;

	CApp11DynamicBuffer<CVector2>& vbPos = sb->getVbPos();

	CVector2* pos = vbPos.getData().get_objects() + ci->vbPosIdx * 4;
	pos[0].set(xa, yTop);
	pos[1].set(xb, yTop);
	pos[2].set(xa, yBot);
	pos[3].set(xb, yBot);
}

int CNIFScrollBar::getScrollBarRange(void)
{
	int len = (iconW << 1) + iconW;		//- 3*iconW  (up + down + slider)
	int r;
	if (bHorizontal) {
		r = x1 - x0;
	}
	else {
		r = y1 - y0;
	}
	return r - len;
}

int CNIFScrollBar::getSliderPos(void)
{
	//
	// transform x0,x1,y0,y1 to resized window space
	//
	CNIFBase::Orig& orig = sb->getAlignOrigs()[alignId];

	//
	// find slider location
	//
	CNIFStaticBuffer::ControlInfo* ci = sb->avlFind(slider_control_id);
	geAssert(ci);

	//
	//do the calculation in resized window space
	//
	if (bHorizontal) {
		int tx0 = x0 + orig.x;
		int sliderCenter = ci->x0 + ((ci->x1 - ci->x0) >> 1);
		return sliderCenter - tx0 - iconW - (iconW>>1);
	}
	else {
		int ty0 = y0 + orig.y;
		int sliderCenter = ci->y0 + ((ci->y1 - ci->y0) >> 1);
		return sliderCenter - ty0 - iconW - (iconW >> 1);
	}
}

/////////////////////////////////////////////////////////////////////////////////
//				CNIFListBox
//
void CNIFListBox::init(CNIFStaticBuffer* sb, unsigned int* ids, unsigned int cnt, 
	int x0, int x1, int y0, int y1, 
	int gap)
{
	this->sb = sb;
	for (unsigned int i = 0; i < cnt; i++) {
		this->ids.push(ids[i]);
	}
	this->x0 = x0;
	this->x1 = x1;
	this->y0 = y0;
	this->y1 = y1;
	this->gap = gap;
}

void CNIFListBox::setText(unsigned int idx, wchar_t* str, unsigned char* colors)
{
	geAssert(idx < ids.get_count());

	unsigned int idc_control = ids[idx];

	if (sb->avlFind(idc_control)) {
		sb->del(idc_control);
	}

	unsigned int cellHeight = sb->getTex().getDynBitmap()->get_cell_height();

	int totalGapHeight = idx * (gap + cellHeight);
	int y = y0 + totalGapHeight;
	sb->addText(idc_control, alignId, str, colors, x0, x1, y, y + cellHeight, true);
}


/////////////////////////////////////////////////////////////////////////////////
//				CNIFComboBox
//
