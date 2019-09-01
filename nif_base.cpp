//*, this is quad/rect-based interface, all quads are stored in either a static class or dynamic class, search support: bvh/avl
//*, dynamic class mainly is used for editbox, static class is for all others.
//*, vertices in quad are pre-tranformed, so, rendering is the fastest, moving is relatively slow, because every vertex needs to be
//	transformed on CPU side then update to GPU, but, it's fine, because transforming is a really simple operation(just an addition)
//*, 

//1, order: the render order is the same as controls's creation order, in case rebuild(), the order is the idc_control





#include "stdafx.h"
#include "ge_util/allocators.h"
#include "app11/app11fontbitmap.h"
#include "math/mathlib.h"

#include "nif_base.h"

#include <tchar.h>

#include "app11/app11tech.h"

extern CApp11AllTechs allTechs;



/////////////////////////////////////////////////////////////////////////////////
//				CNIFFontBitmap
//
//1, square actually is good for sprites.
void CNIFFontBitmap::dump(void)
{
	infos_type::iterator iter;
	infos.set_iterator_to_first(iter);

	for (infos_type::Node* node = infos.get_current_node(iter); node; node = infos.get_next_node(iter)) {
		wchar_t buf[2];
		buf[1] = 0;
		buf[0] = node->key;
		ODS(L"%s", buf);
		ODS(L"(%d)", node->value.ref_count);
	}

	ODS("\n");
}

void CNIFFontBitmap::reset(void)
{
	freeList.cleanup_content();
	infos.reset();
	current_row = 0;
	current_col = 0;
}

bool CNIFFontBitmap::create(int cellHeight, int capacity)
{
	max_col = (UINT)(ceilf(sqrt((float)capacity)));  //make a square bitmap.
	max_row = max_col;

	current_row = 0;
	current_col = 0;

	int w = cellHeight * max_row;
	int h = w;

	HRESULT hr = tex.Create(w, h, D3D11_USAGE_DEFAULT);
	geAssert(hr == S_OK);

	hr = srv.CreateShaderResourceView(tex);
	geAssert(hr == S_OK);

	texelWidth = 1.f / (float)w;
	texelHeight = 1.f / (float)h;

	int hashSize = 2;
	while (hashSize < capacity) {
		hashSize <<= 1;
	}
	infos.init(capacity, hashSize);

	return true;
}

void CNIFFontBitmap::getCharW(int x, int w, float& x0, float& x1)
{
	x0 = x * texelWidth;
	x1 = (x + w) * texelHeight;
}

void CNIFFontBitmap::getCharH(int y, int h, float& y0, float& y1)
{
	y0 = y * texelHeight;
	y1 = (y + h) * texelHeight;
}

bool CNIFFontBitmap::delChar(wchar_t c)
{
	infos_type::Node* node = infos.findNode(c);
	if (node) {
		node->value.ref_count--;
		if (node->value.ref_count == 0) {
			FreeCell cell;
			cell.row = node->value.info.row;
			cell.col = node->value.info.col;
			freeList.push(cell);
			infos.delNode(c);
		}
		return true;
	}
	return false;
}

CNIFFontBitmap::infos_type::Node* CNIFFontBitmap::find(wchar_t c)
{
	infos_type::Node* node = infos.findNode(c);
	return node;
}

unsigned int CNIFFontBitmap::infosPtr2Id(infos_type::Node* node)
{
	return infos.ptr2id(node);
}

CNIFFontBitmap::infos_type::Node* CNIFFontBitmap::infosId2Ptr(unsigned int id)
{
	return infos.id2ptr(id);
}

//1, must first make sure c is not in infos by calling find()
CNIFFontBitmap::infos_type::Node* CNIFFontBitmap::convert(wchar_t c, CApp11FontBitmap::char_info& info)
{
	infos_type::Node* node = infos.newNode(c);
	geAssert(node);		//exceeds the capacity.
	CApp11FontBitmap::char_info& newInfo = node->value.info;

	UINT ch = dynBitmap->get_cell_height();

	bool useOldRowCol = false;

	unsigned int row = current_row;
	unsigned int col = current_col;

	if (freeList.get_count()) {
		useOldRowCol = true;
		FreeCell fc = freeList.pop();
		row = fc.row;
		col = fc.col;
	}

	int dstx = col * ch;
	int dsty = row * ch;

	tex.CopySubresourceRegion2D(dynBitmap->get_bitmap(), info.col * ch, info.row * ch,		//source x,y
		dstx, dsty,		//dest x, y
		(int)(ceilf(info.w)), ch);	//

	float posx_in_pixel = (float)col * ch;
	float posy_in_pixel = (float)row * ch;

	newInfo.ltu = (posx_in_pixel)* texelWidth;
	newInfo.ltv = (posy_in_pixel)* texelWidth;
	newInfo.bru = (posx_in_pixel + info.w) * texelWidth;   //this point has to be at the border of current cell and the next cell.
	newInfo.brv = (posy_in_pixel + ch) * texelWidth;  //this one too

	newInfo.col = col;
	newInfo.row = row;
	newInfo.w = info.w;

	if (!useOldRowCol) {
		current_col++;
		if (current_col >= max_col) {
			current_col = 0;
			current_row++;
		}

		geAssert(current_row < max_row);
	}

	return node;
}

/////////////////////////////////////////////////////////////////////////////////
//					CFixedSizedQuadIb
//
//1, idea: use a long enough quad index buffer to render all kinds of quads -> avoid GS
bool CFixedSizedQuadIb::create(int quadCount)
{
	CScopedFree< FACE_TRIANGLE16>faces;
	faces.allocate(quadCount * 2);
	FACE_TRIANGLE16* pf = faces;
	int idx = 0;
	for (int i = 0; i < quadCount; i++) {
		FACE_TRIANGLE16& fa = pf[i * 2];
		fa.a = idx;
		fa.b = idx + 1;
		fa.c = idx + 2;
		FACE_TRIANGLE16& fb = pf[i * 2 + 1];
		fb.a = idx + 1;
		fb.b = idx + 3;
		fb.c = idx + 2;
		
		idx += 4;
	}
	HRESULT hr = ib.create_ib_immutable(sizeof(FACE_TRIANGLE16), quadCount * 2 * sizeof(FACE_TRIANGLE16), faces);
	geAssert(hr == S_OK);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
//					CNIFBase
//
void CNIFBase::init(CApp11FontBitmapDynamic* dynBitmap, CFixedSizedQuadIb* ib, int cellHeight, int capacity,
	int screenW, int screenH)
{
	this->screenH = screenH;
	this->screenW = screenW;

	calculateAlignsDiff(screenW, screenH);
	calculateAlignsOrig(screenW, screenH);

	tex.init(dynBitmap);

	this->ib = ib;

	bool b = tex.create(cellHeight, capacity);
	geAssert(b);
}

int CNIFBase::getXOriginFromAlign(int align)
{
	if (align < 0)return 0;
	if (align > 0)return screenW;
	return screenW >> 1;
}

int CNIFBase::getYOriginFromAlign(int align)
{
	if (align < 0)return 0;
	if (align > 0)return screenH;
	return screenH >> 1;
}

void CNIFBase::calculateAlignsOrig(int screenW, int screenH)
{
	int xs[3] = { getXOriginFromAlign(-1), getXOriginFromAlign(0), getXOriginFromAlign(1) };
	int ys[3] = { getYOriginFromAlign(-1), getYOriginFromAlign(0), getYOriginFromAlign(1) };

	for (int y = 0; y < 3; y++) {
		Orig* orig = alignOrigs + y * 3;
		for (int x = 0; x < 3; x++) {
			orig[x].x = xs[x];
			orig[x].y = ys[y];
		}
	}
}

// ads must have 9 rooms in it
void CNIFBase::calculateAlignsDiff(int screenW, int screenH)
{
	int hw = screenW >> 1;
	int hh = screenH >> 1;
	int dw = screenW - this->screenW;
	int dh = screenH - this->screenH;
	int hdw = dw >> 1;
	int hdh = dh >> 1;

	int xs[3] = { 0, hdw, dw };
	int ys[3] = { 0, hdh, dh };

	for (int y = 0; y < 3; y++) {
		AlignDiff* diff = alignDiffs + y * 3;
		for (int x = 0; x < 3; x++) {
			diff[x].dx = xs[x];
			diff[x].dy = ys[y];
		}
	}
}

void CNIFBase::dump(CApp11DynamicBuffer<CVector2> &vbPos)
{
	int hw = this->screenW >> 1;
	int hh = this->screenH >> 1;

	float pixelWidth = 2.f / this->screenW;
	float pixelHeight = 2.f / this->screenH;

	CVector2* allVertices = vbPos.getData().get_objects();
	unsigned int cnt = vbPos.getData().get_count();
	int j = 0;
	for (unsigned int i = 0; i < cnt; i++) {
		CVector2 v = allVertices[i];

		v.x += pixelWidth * 0.5f;
		v.x *= hw;
		v.x += hw;

		v.y += pixelHeight * 0.5f;		//shift back
		v.y *= hh;							//to screen space (center (0,0), unit pixel)
		v.y += hh;							//bottom(0);
		v.y = screenH - v.y;			//top(0)		-> this should give the old y coordinate back.

		ODS("(%f,%f)", v.x, v.y);
		if (++j == 4) {
			j = 0;
			ODS("\n");
		}
	}

	ODS("\ndump tex\n");
	tex.dump();
}

/////////////////////////////////////////////////////////////////////////////////
//					CNIFColorIdx
//
void CNIFColorIdx::updateVbColorIdx(void)
{
	vbColorIdx.updateBuffer();
}

void CNIFColorIdx::setColorIdxSlice(unsigned int idx, unsigned char colorIdx, unsigned char slice)
{
	idx <<= 2;
	for (int j = 0; j < 4; j++) {
		Indices& indices = vbColorIdx.getData()[idx + j];
		indices.colorIdx = colorIdx;
		indices.slice = slice;
	}
}

void CNIFColorIdx::pushColorIdxSlice(unsigned char colorIdx, unsigned char slice)
{
	for (int j = 0; j < 4; j++) {
		Indices& indices = vbColorIdx.append();
		indices.colorIdx = colorIdx;
		indices.slice = slice;
	}
}

void CNIFColorIdx::setAllTextColor(unsigned char colorIdx, unsigned int cnt)
{
	vbColorIdx.getData().cleanup_content();
	while (cnt--) {
		Indices& indices = vbColorIdx.append();
		indices.colorIdx = colorIdx;
		indices.slice = 0xff;
	}
}

void CNIFColorIdx::dump(void)
{
	ODS("vbColorIdx [%d/%d]\n", vbColorIdx.getData().get_count(), vbColorIdx.getData().get_cap());
}

/////////////////////////////////////////////////////////////////////////////////
//					CNIFDynamicBuffer
//
void CNIFDynamicBuffer::insert(EditBox* eb, wchar_t c, int pos)
{
	//
	// find c in tex
	//
	CNIFFontBitmap::infos_type::Node* infosNode = tex.find(c);
	if (!infosNode) {
		CApp11FontBitmapDynamic::CHAR_INFO* info = tex.getDynBitmap()->add_word(c);
		geAssert(info);
		geAssert(info->w > 0);		//some character like 'enter' has w of 0.
		infosNode = tex.convert(c, *info);
		infosNode->value.ref_count = 1;
	}
	else {
		infosNode->value.ref_count++;
	}

	//
	// add char to pool
	//
	if (eb->strHeadNode == -1) {
		geAssert(pos == 0);
		pool_type::HeadNode* headNode = pool.newHeadNode();
		Char* pc = pool.addFirst(headNode);
		pc->c = c;
		pc->id = tex.infosPtr2Id(infosNode);

		eb->strHeadNode = pool.headNodePtr2Id(headNode);

		return;
	}

	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);
	pool_type::Node* node = pool.index(headNode, pos);
	Char* pc;
	if (!node) {
		pc = pool.addLast(headNode);
	}
	else {
		pc = pool.insert(headNode, node);
	}

	pc->c = c;
	pc->id = tex.infosPtr2Id(infosNode);
}

void CNIFDynamicBuffer::getStringColorsFromPool(unsigned int headNodeId, CGrowableArrayWithLast<wchar_t>& str)
{
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(headNodeId);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		str.push(node->t.c);
	}

	str.push(0);
}

bool CNIFDynamicBuffer::del(unsigned int idc_editbox)
{
	//
	// search from controls
	//
	EditBox* eb = editboxes.find(idc_editbox);
	if (!eb)return false;

	if (caretEditbox == eb) {
		caretEditbox = NULL;
	}

	//
	// delete from bvh
	//
	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bool b = bvh.del(box, idc_editbox);
	geAssert(b);

	//
	// delete control specific data
	//
	if (eb->strHeadNode != -1) {
		CGrowableArrayWithLast<wchar_t>str;
		getStringColorsFromPool(eb->strHeadNode, str);
		wchar_t* p = str.get_objects();
		while (*p) {
			//delete from font bitmap
			tex.delChar(*p);
			p++;
		}
		pool.delList(eb->strHeadNode);
	}

	//
	// delete from controls
	//
	b = editboxes.del(idc_editbox);
	geAssert(b);

	return true;
}

bool CNIFDynamicBuffer::del(EditBox* eb, int pos)
{
	//
	// find char in pool
	//
	if (eb->strHeadNode == -1) {
		return false;
	}

	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);
	pool_type::Node* node = pool.index(headNode, pos);
	if (!node) {
		return false;		//out of range?
	}
	
	//
	// del char info from tex
	//
	CNIFFontBitmap::infos_type::Node* infosNode = tex.find(node->t.c);
	geAssert(infosNode);
	bool b = tex.delChar(node->t.c);
	geAssert(b);

	//
	// del char from pool
	//
	b = pool.del(headNode, node);
	geAssert(b);

	return b;
}

void CNIFDynamicBuffer::dump(void)
{
	tree_type::iterator iter;
	editboxes.set_iterator_to_first(iter);
	for (EditBox* eb = editboxes.get_current_value(iter); eb; eb = editboxes.get_next_value(iter)) {
		unsigned int idc_text = *editboxes.get_current_key(iter);
		ODS("IDC_TEXT[%d]: (%d,%d)(%d,%d)\n", idc_text, eb->x0, eb->x1, eb->y0, eb->y1);

		GE_MATH::CAabbBox4D box;
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bvh_type::node_pointer_type bvhNode = bvh.find(box, idc_text);
		geAssert(bvhNode);
		ODS("bvhId = %d:\n", bvh.nodePtr2Id(bvhNode));

		if (eb->strHeadNode == -1) {
			ODS("[EMPTY]\n");
			continue;
		}

		pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);
		if (headNode) {
			for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
				ODS(L"%c", node->t);
			}
		}

		ODS("\n");
	}

	CNIFBase::dump(vbPos);
}

void CNIFDynamicBuffer::init(CApp11FontBitmapDynamic* dynBitmap, CFixedSizedQuadIb* ib, 
	int cellHeight, int capacity, int screenW, int screenH)
{
	CNIFBase::init(dynBitmap, ib, cellHeight, capacity, screenW, screenH);

	caretEditbox = NULL;

	CApp11FontBitmap::CHAR_INFO* info = tex.getDynBitmap()->add_word(L'|');	//info is the one in dynBitmap
	CNIFFontBitmap::infos_type::Node* infosNode = tex.convert(L'|', *info); //info has been converted in the one in tex, because I am using this bitmap(local)
	caretInfo = &infosNode->value;
	caretInfo->ref_count = 1;
}

void CNIFDynamicBuffer::moveEditBox(unsigned int idc_editbox, int dx, int dy)
{
	//
	// del in bvh
	//
	EditBox* eb = editboxes.find(idc_editbox);

	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bool b = bvh.del(box, idc_editbox);
	geAssert(b);

	//
	// move in avl
	//
	eb->x0 += dx;
	eb->y0 += dy;
	eb->x1 += dx;
	eb->y1 += dy;

	//
	// add back to bvh
	//
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;

	bvh_type::node_pointer_type node = bvh.add(box);
	node->t = idc_editbox;
}

void CNIFDynamicBuffer::addEditBox(unsigned int idc_editbox, unsigned int alignId, int x0, int x1, int y0, int y1)
{
	//
	// transform form aligned space to world space.
	//
	Orig& orig = alignOrigs[alignId];

	//
	// add to avl tree
	//
	EditBox* eb = editboxes.add(idc_editbox);
	eb->x0 = x0 + orig.x;
	eb->y0 = y0 + orig.y;
	eb->x1 = x1 + orig.x;
	eb->y1 = y1 + orig.y;
	eb->strHeadNode = -1;
	eb->alignId = alignId;

	//
	// add to bvh
	//
	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bvh_type::node_pointer_type node = bvh.add(box);
	node->t = idc_editbox;
}

int CNIFDynamicBuffer::getCharPosInText(EditBox* eb, int pos)
{
	if (eb->strHeadNode == -1) {
		geAssert(pos == 0);
		return 0;
	}

	int i = 0;
	float w = 0;
	for (pool_type::Node* node = pool.getFirstNode(eb->strHeadNode); node && i++<pos; node = pool.getNextNode(node)) {

		CNIFFontBitmap::infos_type::Node* infosNode = tex.infosId2Ptr(node->t.id);
		CNIFFontBitmap::CHAR_INFO* cinfo = &infosNode->value;
		w += cinfo->info.w;
	}
	
	return (int)w;
}

void CNIFDynamicBuffer::updateAll(void)
{
	vbPos.reset();
	vbUv.reset();

	updateCaretPosUv(caretEditbox, caretPos);
	updateEditboxesData();
	updateVb();
}

bool CNIFDynamicBuffer::on_char_del(void)
{
	if (!caretEditbox)return false;

	bool b = del(caretEditbox, caretPos);
	if (!b)return false;

	updateAll();

	return true;
}

bool CNIFDynamicBuffer::on_char_backspace(void)
{
	if (!caretEditbox)return false;

	if (caretPos == 0)return false;

	bool b = del(caretEditbox, caretPos - 1);

	caretPos--;

	updateAll();

	return true;
}

bool CNIFDynamicBuffer::on_char(WPARAM wParam, LPARAM lParam)
{
	if (!caretEditbox)return false;

	insert(caretEditbox, wParam, caretPos);

	advanceCaret();

	updateAll();

	return true;
}

void CNIFDynamicBuffer::updateEditboxesData(EditBox* eb, float pixelWidth, float pixelHeight, float hpw, float hph)
{
	if (eb->strHeadNode == -1)return;

	//
	// screen space lefttop(0,0) to center(0,0)
	//
	int x0 = eb->x0;
	int x1 = eb->x1;
	int y0 = eb->y0;
	int y1 = eb->y1;

	x0 -= screenW >> 1;			//from left(0) to center(0)
	x1 -= screenW >> 1;
	y0 = screenH - y0;			//from bottom(0) to top(0), axis goes top-down
	y1 = screenH - y1;
	y0 -= screenH >> 1;			//from top(0) to center(0), axis goes top-down
	y1 -= screenH >> 1;

	//
	//
	//
	float x = (float)x0;
	float yTop = y0 * pixelHeight;

	float h = (float)temp_min<int>(tex.getDynBitmap()->get_cell_height(), y0 - y1);

	float yBot = (y0 - h) * pixelHeight;
	float xa = x * pixelWidth;
	float xb;

	yTop -= hph;		//TODO: is this shifting really still needed??? (microsoft map texel to pixel)
	xa -= hpw;
	yBot -= hph;

	float xEnd = x1 * pixelWidth - hpw;

	//
	// push to vb
	//
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);
	if (headNode) {
		for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
			CNIFFontBitmap::infos_type::Node* infosNode = tex.infosId2Ptr(node->t.id);
			CNIFFontBitmap::CHAR_INFO* cinfo = &infosNode->value;
			CApp11FontBitmap::CHAR_INFO* info = &cinfo->info;

			x += info->w;
			xb = x * pixelWidth;
			xb -= hpw;

			if (xb > xEnd) {
				break;
			}

			CVector2 v, uv;

			v.set(xa, yTop); vbPos.push(v);
			uv.set(info->ltu, info->ltv); vbUv.push(uv);		//left top

			v.set(xb, yTop);  vbPos.push(v);
			uv.set(info->bru, info->ltv);  vbUv.push(uv);		//right top

			v.set(xa, yBot);  vbPos.push(v);
			uv.set(info->ltu, info->brv);  vbUv.push(uv);		//left bottom

			v.set(xb, yBot);  vbPos.push(v);
			uv.set(info->bru, info->brv);  vbUv.push(uv);		//right bottom

			xa = xb;
		}
	}
}

void CNIFDynamicBuffer::updateEditboxesData(void)
{
	float pixelHeight = 2.f / screenH;
	float pixelWidth = 2.f / screenW;
	float hph = 0.5f * pixelHeight;
	float hpw = 0.5f * pixelWidth;

	tree_type::iterator iter;
	editboxes.set_iterator_to_first(iter);
	for (EditBox* eb = editboxes.get_current_value(iter); eb; eb = editboxes.get_next_value(iter)) {
		updateEditboxesData(eb, pixelWidth, pixelHeight, hpw, hph);
	}
}

void CNIFDynamicBuffer::advanceCaret(void)
{
	caretPos++;
}

//1, if only the caret's pos has been changed, then, no need to update the rest, but it's a little mistake-prone here.
//2, in other cases, deletion/insertion of characters, entire buffer needs to be updated.
//3, this improves performance a little bit, but leaves room for mistakes, if it's not worth it, just remove bcreate. update all at all times.
void CNIFDynamicBuffer::updateCaretPosUv(EditBox* caretEditbox, int caretPos)
{
	if (!caretEditbox) {
		CVector2 v, uv;
		v.set(-2222, -2222);		//a random invisible dot.
		if (vbPos.getData().get_count()) {
			for (int i = 0; i < 4; i++) {
				vbUv.getData()[i] = uv;
				vbPos.getData()[i] = v;
			}
		}
		else {
			//create a fake one as place holder.
			for (int i = 0; i < 4; i++) {
				vbUv.push(uv);
				vbPos.push(v);
			}
		}
		return;
	}

	float pixelHeight = 2.f / screenH;
	float pixelWidth = 2.f / screenW;
	float hph = 0.5f * pixelHeight;
	float hpw = 0.5f * pixelWidth;

	bool bcreate = true;
	if (vbPos.getData().get_count()) {
		bcreate = false;
	}

	//
	// find caret screen pos
	//
	int w = getCharPosInText(caretEditbox, caretPos);

	//
	// screen space lefttop(0,0) to center(0,0)
	//
	int x0 = caretEditbox->x0 + w;
	int y0 = caretEditbox->y0;
	int x1 = x0 + (int)caretInfo->info.w;
	int y1 = caretEditbox->y1;

	x0 -= screenW >> 1;			//from left(0) to center(0)
	x1 -= screenW >> 1;
	y0 = screenH - y0;			//from bottom(0) to top(0), axis goes top-down
	y1 = screenH - y1;
	y0 -= screenH >> 1;			//from top(0) to center(0), axis goes top-down
	y1 -= screenH >> 1;

	//
	// 
	//
	float fx0 = x0 * pixelWidth;
	float fx1 = x1 * pixelWidth;
	float fy0 = y0 * pixelHeight;
	float fy1 = y1 * pixelHeight;

	fx0 -= hpw;
	fx1 -= hpw;
	fy0 -= hph;
	fy1 -= hph;

	CVector2 v[4], uv[4];

	CApp11FontBitmap::CHAR_INFO* info = &caretInfo->info;

	v[0].set(fx0, fy0); 
	uv[0].set(info->ltu, info->ltv); 		//left top

	v[1].set(fx1, fy0);
	uv[1].set(info->bru, info->ltv);  		//right top		

	v[2].set(fx0, fy1);
	uv[2].set(info->ltu, info->brv); 		//left bottom	

	v[3].set(fx1, fy1);
	uv[3].set(info->bru, info->brv);  		//right bottom

	if (bcreate) {
		for (int i = 0; i < 4; i++) {
			vbUv.push(uv[i]);
			vbPos.push(v[i]);
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			vbUv.getData()[i] = uv[i];
			vbPos.getData()[i] = v[i];
		}
	}
}

CNIFDynamicBuffer::EditBox* CNIFDynamicBuffer::findEditBox(unsigned int id)
{
	return editboxes.find(id);
}


extern void set_info(unsigned int idx, const wchar_t* format, ...);

void CNIFDynamicBuffer::setCaret(int x, int y)
{
	//
	// locate the editbox
	//
	query_result.cleanup_content();

	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)x;
	box.vmax.x = (float)x;
	box.vmin.z = (float)y;
	box.vmax.z = (float)y;

	bvh.queryBoxXZ(box, query_result);

	unsigned int cnt = query_result.get_count();

#if 0
	if (cnt > 0) {
		bvh_type::node_pointer_type bvhNode = query_result[0];
		set_info(2, L"(%d)in(%d,%d), (%d)in(%d,%d)", 
			x, (int)bvhNode->bbox.vmin.x, (int)bvhNode->bbox.vmax.x,
			y, (int)bvhNode->bbox.vmin.z, (int)bvhNode->bbox.vmax.z);
	}
	else {
		set_info(2, L"no intersection");
	}
#endif

	if (cnt == 0) {
		caretEditbox = NULL;
		return;
	}

	bvh_type::node_pointer_type node = query_result[0];

	EditBox* oldCaretEditbox = caretEditbox;
	caretEditbox = editboxes.find(node->t);
	geAssert(caretEditbox);

	//
	// locate the char
	//
	int dx = x - caretEditbox->x0;
	geAssert(dx >= 0);

	if (caretEditbox->strHeadNode == -1) {
		caretPos = 0;
		updateCaretPosUv(caretEditbox, caretPos);
		updateVb();
		return;
	}

	float x0 = 0;

	int oldCaretPos = caretPos;
	caretPos = 0;
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(caretEditbox->strHeadNode);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		CNIFFontBitmap::infos_type::Node* infosNode = tex.infosId2Ptr(node->t.id);
		CNIFFontBitmap::CHAR_INFO* cinfo = &infosNode->value;
		CApp11FontBitmap::CHAR_INFO* info = &cinfo->info;

		x0 += info->w;
		if (x0 >= dx)break;
		caretPos++;
	}

	if (caretPos == oldCaretPos && oldCaretEditbox == caretEditbox) {
		return;
	}

	//no update editboxdata needed.
	updateCaretPosUv(caretEditbox, caretPos);
	updateVb();
}

void CNIFDynamicBuffer::updateVb(void)
{
	vbPos.updateBuffer();
	vbUv.updateBuffer();
	CNIFColorIdx::updateVbColorIdx();
}

void CNIFDynamicBuffer::render(void)
{
	unsigned int quadCnt = vbPos.getData().get_count() >> 2;
	unsigned int faceIdxCnt = quadCnt * 6;

	vbPos.getBuffer().IASetVertexBuffers(0, 1);
	vbUv.getBuffer().IASetVertexBuffers(1, 1);
	vbColorIdx.getBuffer().IASetVertexBuffers(2, 1);

	allTechs.nif_font->getCb0().VSSetConstantBuffers(0, 1);

	allTechs.nif_font->set();

	tex.getSrv().PSSetShaderResources();

	CDX11Device::immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CDX11Device::immediate_context->DrawIndexed(faceIdxCnt, 0, 0);
}

void CNIFDynamicBuffer::resize(int screenW, int screenH)
{
	//
	// resize orgins. (for BVH)
	//
	calculateAlignsDiff(screenW, screenH);
	calculateAlignsOrig(screenW, screenH);

	//
	// adjust in avl and bvh
	//
	tree_type::iterator iter;
	editboxes.set_iterator_to_first(iter);
	for (EditBox* eb = editboxes.get_current_value(iter); eb; eb = editboxes.get_next_value(iter)) {
		unsigned int idc_text = *editboxes.get_current_key(iter);

		//
		// delete old box from bvh
		//
		GE_MATH::CAabbBox4D box;
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bool b = bvh.del(box, idc_text);
		geAssert(b);

		//
		// adjust text box
		//
		AlignDiff& df = alignDiffs[eb->alignId];

		eb->x0 += df.dx;
		eb->x1 += df.dx;
		eb->y0 += df.dy;
		eb->y1 += df.dy;

		//
		// add new box to bvh
		//
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bvh_type::node_pointer_type node = bvh.add(box);
		node->t = idc_text;
	}

	//
	//	this resize() is different from CNIFStaticBuffer(the static one), here, df is not used, just re-calculate everything.
	//
	this->screenW = screenW;
	this->screenH = screenH;

	// 
	// resize in vbPos.
	//
	vbPos.reset();
	vbUv.reset();

	updateCaretPosUv(caretEditbox, caretPos);
	updateEditboxesData();
	//
	//
	//
	updateVb();
}


/////////////////////////////////////////////////////////////////////////////////
//					CNIFStaticBuffer
//
//1, int x0, int x1, int y0, int y1 -> target box, and they are in screen space (topleft (0,0))
bool CNIFStaticBuffer::changeTextColor(unsigned int idc_control, unsigned char* colors, unsigned int cnt)
{
	ControlInfo* eb = controls.find(idc_control);
	geAssert(eb);

	return changeTextColor(eb, colors, cnt);
}

bool CNIFStaticBuffer::changeTextColor(unsigned int idc_control, unsigned char colorIdx)
{
	ControlInfo* eb = controls.find(idc_control);
	geAssert(eb);

	return changeTextColor(eb, colorIdx);
}

// problem: if not all text is displayed.
bool CNIFStaticBuffer::changeTextColor(ControlInfo* eb, unsigned char* colors, unsigned int cnt)
{
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);

	geAssert(pool.count(headNode) == cnt);

	int i = 0;
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; i++, node = pool.getNextNode(node)) {
		node->t.colorIdx = colors[i];
	}

	for (unsigned int i = 0; i < cnt; i++) {
		unsigned int idx = eb->vbPosIdx * 4 + i * 4;
		geAssert(idx + 4 <= vbColorIdx.getData().get_count());
		Indices* indices = vbColorIdx.getData().get_objects() + idx;
		unsigned int c = colors[i];
		for (int j = 0; j < 4; j++) {
			indices[j].colorIdx = c;
		}
	}

	return true;
}

// problem: if not all text is displayed.
bool CNIFStaticBuffer::changeTextColor(ControlInfo* eb, unsigned char colorIdx)
{
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(eb->strHeadNode);

	unsigned int cnt = 0;
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		node->t.colorIdx = colorIdx;
		cnt++;
	}

	for (unsigned int i = 0; i < cnt; i++) {
		unsigned int idx = eb->vbPosIdx * 4 + i * 4;
		geAssert(idx + 4 <= vbColorIdx.getData().get_count());
		Indices* indices = vbColorIdx.getData().get_objects() + idx;
		for (int j = 0; j < 4; j++) {
			indices[j].colorIdx = colorIdx;
		}
	}

	return true;
}

//dx in clip space
void CNIFStaticBuffer::shiftTextX(ControlInfo* ci, int cnt, float dx)
{
	CVector2* poses = vbPos.getData().get_objects();
	poses += ci->vbPosIdx * 4;
	for (int i = 0; i < cnt; i++) {
		for (int j = 0; j < 4; j++) {
			poses->x += dx;
			poses++;
		}
	}
}

//oldAlignedId, newAlignId are all relative to text box(x0,x1,y0,y1), not screen alignment.
void CNIFStaticBuffer::alignText(ControlInfo* ci, int oldAlignedId, int newAlignId)
{
	if (oldAlignedId == newAlignId) {
		return;
	}

	int strLen;
	int strW = getTextLenInPixel(ci, strLen);
	int rectW = ci->x1 - ci->x0;
	int halfRectW = rectW >> 1;
	int halfStrW = strW >> 1;

	int alignOldX0;
	switch (oldAlignedId) {
	case -1:		//left
		alignOldX0 = 0;
		break;
	case 0:			//center
		alignOldX0 = halfRectW - halfStrW;
		break;
	case 1:			//right
		alignOldX0 = rectW - strW;
		break;
	}
	
	int alignNewX0;
	switch (newAlignId) {
	case -1:		//left
		alignNewX0 = 0;
		break;
	case 0:			//center
		alignNewX0 = halfRectW - halfStrW;
		break;
	case 1:			//right
		alignNewX0 = rectW - strW;
		break;
	}

	float dx = (float)(alignNewX0 - alignOldX0);

	float pixelWidth = 2.f / screenW;

	dx *= pixelWidth;
	dx -= pixelWidth * 0.5f;

	shiftTextX(ci, strLen, dx);
}

int CNIFStaticBuffer::getTextLenInPixel(ControlInfo* ci, int&cnt)
{
	cnt = 0;
	float len = 0;
	geAssert(ci->strHeadNode != -1);
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(ci->strHeadNode);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		CNIFFontBitmap::infos_type::Node* infosNode = tex.infosId2Ptr(node->t.id);
		CNIFFontBitmap::CHAR_INFO* cinfo = &infosNode->value;
		len += cinfo->info.w;
		cnt++;
	}
	return (int)len;
}

//addText add char to tex and increase ref_count.
//then add char to pool for headNode.
void CNIFStaticBuffer::addText(pool_type::HeadNode* headNode, wchar_t* str, unsigned char* colors, int strLen)
{
	int i = 0;
	while (*str) {
		CNIFFontBitmap::infos_type::Node* infosNode = tex.find(*str);
		if (!infosNode) {
			CApp11FontBitmap::CHAR_INFO* info = tex.getDynBitmap()->add_word(*str);	//info is the one in dynBitmap
			infosNode = tex.convert(*str, *info);			//info has been converted in the one in tex, because I am using this bitmap(local)
			infosNode->value.ref_count = 1;					//if font has just been created, ref will have to be increased all times.
		}
		else {
			CNIFFontBitmap::CHAR_INFO* nif_char_info = &infosNode->value;
			nif_char_info->ref_count++;
		}

		//
		// add char to pool
		//
		Char* pc;
		pc = pool.addLast(headNode);
		pc->c = *str;
		pc->id = tex.infosPtr2Id(infosNode);
		pc->colorIdx = colors[i++];		//align is in the head

		str++;
	}
}

//replaceText is faster than del() then add() sometimes, if most char in the deleted is still in newly added.
//after replaceText(), rebuild() must be called to reflect changes.
bool CNIFStaticBuffer::replaceText(ControlInfo* ci, wchar_t* str, unsigned char* colors, int strLen)
{
	unsigned int oldStrHeadNodeId = ci->strHeadNode;

	//
	// assign a newly allocated to eb
	//
	pool_type::HeadNode* headNode = pool.newHeadNode();
	ci->strHeadNode = pool.headNodePtr2Id(headNode);

	//
	// add str to tex
	//
	addText(headNode, str, colors, strLen);

	//
	// remove old string
	//
	delText(oldStrHeadNodeId);

	return true;
}

//1, addToAvlBvh is false usually on rebuilding.
//2, createFont: sometimes, fontbitmap has already been created for this text control, user doesn't want anything in font bitmap to change.
//	for example, delete a text control, maybe then add another one back in, after this rebuild_from_avl is called, 
//	in this case, font data need not to be changed at all.  -> this usually only can be false when used from create_from_avl()
bool CNIFStaticBuffer::addText(unsigned int idc_control, unsigned int alignId, char localAlign, wchar_t* str, unsigned char* colors,
	int x0, int x1, int y0, int y1, bool addToAvlBvh, bool createFont, int strLen, int&added, bool autoWrap, int& totalLines)
{
	totalLines = 1;

	//
	// from aligned/object space to world space. (screen space)
	//
	Orig& orig = alignOrigs[alignId];

	x0 += orig.x;
	x1 += orig.x;
	y0 += orig.y;
	y1 += orig.y;

	pool_type::HeadNode* headNode;
	ControlInfo* eb;
	if (addToAvlBvh) {
		//
		// add to avl tree
		//
		eb = controls.add(idc_control);
		geAssert(eb);
		eb->x0 = x0;
		eb->y0 = y0;
		eb->x1 = x1;
		eb->y1 = y1;
		headNode = pool.newHeadNode();
		eb->strHeadNode = pool.headNodePtr2Id(headNode);
		eb->align = alignId;
		eb->localAlign = localAlign;
		eb->type = 0;

		eb->slice = 255;

		//
		// add to bvh
		//
		GE_MATH::CAabbBox4D box;
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bvh_type::node_pointer_type node = bvh.add(box);
		node->t = idc_control;
	}
	else {
		eb = controls.find(idc_control);
		geAssert(eb);
	}

	//
	// advance vbPosIdx
	//
	eb->vbPosIdx = vbPosIdx;

	//
	//transform coordinates from lefttop(0,0) to center(0,0)
	//
	x0 -= screenW >> 1;			//from left(0) to center(0)
	x1 -= screenW >>1;
	y0 = screenH - y0;			//from bottom(0) to top(0), axis goes top-down
	y1 = screenH - y1;
	y0 -= screenH >>1;			//from top(0) to center(0), axis goes top-down
	y1 -= screenH >>1;

	//
	//
	//
	float pixelHeight = 2.f / screenH;
	float pixelWidth = 2.f / screenW;

	//float h = (float)tex.getDynBitmap()->get_cell_height();
	float x = (float)x0;
	float yTop = y0 * pixelHeight;

	float h = (float)temp_min<int>(tex.getDynBitmap()->get_cell_height(), y0 - y1);
	
	float yBot = (y0 - h) * pixelHeight;
	float xa = x * pixelWidth;
	float xb;

	yTop -= pixelHeight * 0.5f;		//TODO: is this shifting really still needed??? (microsoft map texel to pixel)
	xa -= pixelWidth * 0.5f;
	yBot -= pixelHeight * 0.5f;

	float xEnd = x1 * pixelWidth - pixelWidth * 0.5f;

	CVector2 v;
	CVector2 uv;

	unsigned int colorIdx = 0;

	pool.reserveNode(strLen);

	added = 0;
	while (*str) {
		//no need to worry about ref_count in static bitmap.
		CNIFFontBitmap::infos_type::Node* infosNode = tex.find(*str);
		CApp11FontBitmap::CHAR_INFO* info;
		if (!infosNode) {
			geAssert(createFont);		
			info = tex.getDynBitmap()->add_word(*str);	//info is the one in dynBitmap
			infosNode = tex.convert(*str, *info);			//info has been converted in the one in tex, because I am using this bitmap(local)
			infosNode->value.ref_count = 1;		//if font has just been created, ref will have to be increased all times.
		}
		else {
			CNIFFontBitmap::CHAR_INFO* nif_char_info = &infosNode->value;
			if (createFont) {
				nif_char_info->ref_count++;
			}
		}
		info = &infosNode->value.info;

		Char* pc;
		if (addToAvlBvh) {
			//
			// add char to pool
			//
			pc = pool.addLast(headNode);
			pc->c = *str;
			pc->id = tex.infosPtr2Id(infosNode);
		}

		x += info->w;
		xb = x * pixelWidth;
		xb -= pixelWidth * 0.5f;

		if (xb > xEnd) {
			if (autoWrap) {
				x = (float)x0;
				xa = x * pixelWidth;
				
				x += info->w;
				xb = x * pixelWidth;

				yTop = (y0 - h * totalLines) * pixelHeight;
				yBot = (y0 - h * (totalLines + 1)) * pixelHeight;

				xa -= pixelWidth * 0.5f;
				xb -= pixelWidth * 0.5f;
				yTop -= pixelHeight * 0.5f;
				yBot -= pixelHeight * 0.5f;

				totalLines++;
			}
			else {
				break;
			}
		}

		added++;

		v.set(xa, yTop); vbPos.push(v);
		uv.set(info->ltu, info->ltv); vbUv.push(uv);		//left top

		v.set(xb, yTop);  vbPos.push(v);
		uv.set(info->bru, info->ltv);  vbUv.push(uv);		//right top

		v.set(xa, yBot);  vbPos.push(v);
		uv.set(info->ltu, info->brv);  vbUv.push(uv);		//left bottom

		v.set(xb, yBot);  vbPos.push(v);
		uv.set(info->bru, info->brv);  vbUv.push(uv);		//right bottom

		unsigned char color = colors[colorIdx++];
		for (int j = 0; j < 4; j++) {
			alignIds.push(alignId);

			Indices& indice = vbColorIdx.append();
			indice.colorIdx = color;
			indice.slice = 0xff;
		}

		if (addToAvlBvh) {
			pc->colorIdx = color;
		}

		str++;

		xa = xb;
	}

	vbPosIdx += added;

	if (localAlign != -1) {		//-1 means left
		alignText(eb, -1, localAlign);
	}

	return true;
}

unsigned int CNIFStaticBuffer::reserveQuad(void)
{
	for (int j = 0; j < 4; j++) {
		//
		CVector2 v, uv;
		vbPos.push(v);
		vbUv.push(uv);

		//push 
		alignIds.push(0);

		//
		Indices& indice = vbColorIdx.append();
		indice.colorIdx = 0;		//white
		indice.slice = 0;
	}
	return vbPosIdx++;
}

//1, addToAvlBvh is false usually on rebuilding.
//2, reservedVbPos: 
void CNIFStaticBuffer::addImage(unsigned int idc_control, unsigned int alignId, unsigned char slice, unsigned char colorIdx,
	float ltu, float ltv, float bru, float brv, 
	int x0, int x1, int y0, int y1, bool addToAvlBvh, unsigned int reservedVbPos)
{
	Orig& orig = alignOrigs[alignId];

	x0 += orig.x;
	x1 += orig.x;
	y0 += orig.y;
	y1 += orig.y;

	ControlInfo* eb;
	if (addToAvlBvh) {
		//
		// add to avl tree
		//
		eb = controls.add(idc_control);
		eb->x0 = x0;
		eb->y0 = y0;
		eb->x1 = x1;
		eb->y1 = y1;
		pool_type::HeadNode* headNode = pool.newHeadNode();
		eb->strHeadNode = pool.headNodePtr2Id(headNode);
		eb->align = alignId;
		eb->type = 1;
		eb->colorIdx = colorIdx;

		eb->bru = bru;
		eb->brv = brv;
		eb->ltu = ltu;
		eb->ltv = ltv;

		eb->slice = slice;

		//
		// add to bvh
		//
		GE_MATH::CAabbBox4D box;
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bvh_type::node_pointer_type node = bvh.add(box);
		node->t = idc_control;
	}
	else {
		eb = controls.find(idc_control);
		geAssert(eb);
	}

	//
	// advance vbPosIdx
	//
	if (reservedVbPos == -1) {
		eb->vbPosIdx = vbPosIdx++;
	}

	//
	//transform coordinates from lefttop(0,0) to center(0,0)
	//
	x0 -= screenW >> 1;			//from left(0) to center(0)
	x1 -= screenW >> 1;
	y0 = screenH - y0;			//from bottom(0) to top(0), axis goes top-down
	y1 = screenH - y1;
	y0 -= screenH >> 1;			//from top(0) to center(0), axis goes top-down
	y1 -= screenH >> 1;

	//
	//
	//
	float pixelHeight = 2.f / screenH;
	float pixelWidth = 2.f / screenW;

	float yTop = y0 * pixelHeight;
	float yBot = y1 * pixelHeight;
	float xa = x0 * pixelWidth;
	float xb = x1 * pixelWidth;

	yTop -= pixelHeight * 0.5f;		//TODO: is this shifting really still needed??? (microsoft map texel to pixel)
	yBot -= pixelHeight * 0.5f;
	xa -= pixelWidth * 0.5f;
	xb -= pixelWidth * 0.5f;

	if (reservedVbPos == -1) {
		CVector2 v, uv;
		v.set(xa, yTop); vbPos.push(v);
		uv.set(ltu, ltv); vbUv.push(uv);		//left top

		v.set(xb, yTop);  vbPos.push(v);
		uv.set(bru, ltv);  vbUv.push(uv);		//right top

		v.set(xa, yBot);  vbPos.push(v);
		uv.set(ltu, brv);  vbUv.push(uv);		//left bottom

		v.set(xb, yBot);  vbPos.push(v);
		uv.set(bru, brv);  vbUv.push(uv);		//right bottom

		for (int j = 0; j < 4; j++) {
			alignIds.push(alignId);

			Indices& indice = vbColorIdx.append();
			indice.colorIdx = colorIdx;		
			indice.slice = slice;
		}
	}
	else {
		unsigned int idx = reservedVbPos * 4;
		for (int j = 0; j < 4; j++) {
			alignIds[idx + j] = alignId;

			Indices& indice = vbColorIdx.getData()[idx + j];
			indice.colorIdx = colorIdx;		
			indice.slice = slice;
		}

		CVector2 v, uv;
		v.set(xa, yTop); vbPos.getData()[idx] = v;
		uv.set(ltu, ltv); vbUv.getData()[idx] = uv;		//left top
		idx++;

		v.set(xb, yTop);  vbPos.getData()[idx] = v;
		uv.set(bru, ltv);  vbUv.getData()[idx] = uv;			//right top
		idx++;

		v.set(xa, yBot);  vbPos.getData()[idx] = v;
		uv.set(ltu, brv);  vbUv.getData()[idx] = uv;		//left bottom
		idx++;

		v.set(xb, yBot);  vbPos.getData()[idx] = v;
		uv.set(bru, brv);  vbUv.getData()[idx] = uv;		//right bottom
	}
}

//void addImage(unsigned int alignId, unsigned char slice, float ltu, float ltv, float bru, float brv, int x0, int x1, int y0, int y1);
//x, y: leftop(0,0)
//1, addToAvlBvh is false usually on rebuilding.
void CNIFStaticBuffer::addSquardIcon(unsigned int idc_control, unsigned int alignId, unsigned char colorIdx,
	unsigned char slice, int imageW, int iconW, int iconx, int icony, int x, int y, bool addToAvlBvh)
{
	float iw = (float)iconW / (float)imageW;

	return addImage(idc_control, alignId, slice, colorIdx, iconx * iw, icony * iw, (iconx + 1) * iw, (icony + 1) * iw, x, x + iconW, y, y + iconW, addToAvlBvh, -1);
}

void CNIFStaticBuffer::delText(unsigned int strHeadNodeId)
{
	geAssert(strHeadNodeId != -1);
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(strHeadNodeId);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		bool b = tex.delChar(node->t.c);
		geAssert(b);
	}
	pool.delList(strHeadNodeId);
}

bool CNIFStaticBuffer::del(unsigned int idc_control)
{
	//
	// search from controls
	//
	ControlInfo* eb = controls.find(idc_control);
	if (!eb)return false;

	//
	// delete from bvh
	//
	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bool b = bvh.del(box, idc_control);
	geAssert(b);

	//
	// delete control specific data
	//
	//CGrowableArrayWithLast<wchar_t>str;
	//CGrowableArrayWithLast<unsigned char>colors;
	//wchar_t* p;
	switch (eb->type) {
	case 0:
		delText(eb->strHeadNode);
		/*getStringColorsFromPool(eb->strHeadNode, str, colors);
		p = str.get_objects();
		while (*p) {
			//delete from font bitmap
			tex.delChar(*p);
			p++;
		}
		pool.delList(eb->strHeadNode);*/
		break;
	case 1:
		break;
	default:
		geBreak(1);
		break;
	}

	//
	// delete from controls
	//
	b = controls.del(idc_control);
	geAssert(b);

	return true;
}

void CNIFStaticBuffer::updateVbPos(void)
{
	vbPos.updateBuffer();
}

void CNIFStaticBuffer::updateVbPosUvColor(void)
{
	vbPos.updateBuffer();
	vbUv.updateBuffer();
	CNIFColorIdx::updateVbColorIdx();
}

void CNIFStaticBuffer::resize(int screenW, int screenH)
{
	//
	// resize orgins.
	//
	calculateAlignsDiff(screenW, screenH);
	calculateAlignsOrig(screenW, screenH);

	//
	// adjust in avl and bvh
	//
	tree_type::iterator iter;
	controls.set_iterator_to_first(iter);
	for (ControlInfo* eb = controls.get_current_value(iter); eb; eb = controls.get_next_value(iter)) {
		unsigned int idc_text = *controls.get_current_key(iter);

		//
		// delete old box from bvh
		//
		GE_MATH::CAabbBox4D box;
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bool b = bvh.del(box, idc_text);
		geAssert(b);

		//
		// adjust text box
		//
		AlignDiff& df = alignDiffs[eb->align];

		eb->x0 += df.dx;
		eb->x1 += df.dx;
		eb->y0 += df.dy;
		eb->y1 += df.dy;

		//
		// add new box to bvh
		//
		box.vmin.x = (float)eb->x0;
		box.vmax.x = (float)eb->x1;
		box.vmin.z = (float)eb->y0;
		box.vmax.z = (float)eb->y1;
		box.vmin.y = 0;
		box.vmax.y = 0;

		bvh_type::node_pointer_type node = bvh.add(box);
		node->t = idc_text;
	}

	//
	//
	//
	float oldPixelWidth = 2.f / this->screenW;
	float oldPixelHeight = 2.f / this->screenH;

	float newPixelWidth = 2.f / screenW;
	float newPixelHeight = 2.f / screenH;

	int hw = this->screenW >> 1;
	int hh = this->screenH >> 1;

	int newHw = screenW >> 1;
	int newHh = screenH >> 1;

	CVector2* allVertices = vbPos.getData().get_objects();
	unsigned int cnt = vbPos.getData().get_count();
	for(unsigned int i=0;i<cnt;i++){
		AlignDiff& df = alignDiffs[alignIds[i]];

		CVector2& v = allVertices[i];
		v.x += oldPixelWidth * 0.5f;		//shift by 0.5f
		v.x *= hw;							//to screen space (center (0,0), unit pixel)
		v.x += hw;							//this should give the old x coordinate back.

		v.x += df.dx;

		v.x -= newHw;
		v.x *= newPixelWidth;
		v.x -= newPixelWidth * 0.5f;

		v.y += oldPixelHeight * 0.5f;		//shift back
		v.y *= hh;							//to screen space (center (0,0), unit pixel)
		v.y += hh;							//bottom(0);
		v.y = this->screenH - v.y;			//top(0)		-> this should give the old y coordinate back.

		v.y += df.dy;

		v.y = screenH - v.y;
		v.y -= newHh;
		v.y *= newPixelHeight;
		v.y -= newPixelHeight * 0.5f;
	}

	this->screenW = screenW;
	this->screenH = screenH;

	vbPos.updateBuffer();
}

void CNIFStaticBuffer::render(void)
{
	unsigned int quadCnt = vbPos.getData().get_count() >> 2;
	unsigned int faceIdxCnt = quadCnt * 6;

	vbPos.getBuffer().IASetVertexBuffers(0, 1);
	vbUv.getBuffer().IASetVertexBuffers(1, 1);
	vbColorIdx.getBuffer().IASetVertexBuffers(2, 1);

	allTechs.nif_font->getCb0().VSSetConstantBuffers(0, 1);

	allTechs.nif_font->set();

	tex.getSrv().PSSetShaderResources();

	CDX11Device::immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CDX11Device::immediate_context->DrawIndexed(faceIdxCnt, 0, 0);
}

void CNIFStaticBuffer::moveAllControlsInBvh(bvh_type::Node* node, int dx, int dy)
{
	node->bbox.vmin.x += dx;
	node->bbox.vmin.z += dy;
	node->bbox.vmax.x += dx;
	node->bbox.vmax.z += dy;

	if (node->left)moveAllControlsInBvh(node->left, dx, dy);
	if (node->right)moveAllControlsInBvh(node->right, dx, dy);
}

void CNIFStaticBuffer::moveAllControls(int dx, int dy)
{
	moveAllControlsInBvh(bvh.getRoot(), dx, dy);
	
	tree_type::iterator iter;
	controls.set_iterator_to_first(iter);
	for (ControlInfo* ci = controls.get_current_value(iter); ci; ci = controls.get_next_value(iter)) {
		ci->x0 += dx;
		ci->y0 += dy;
		ci->x1 += dx;
		ci->y1 += dy;
	}
}

//rebuild is needed afterwards for this to take effect.
void CNIFStaticBuffer::moveControl(unsigned int idc_control, int dx, int dy)
{
	//
	// del in bvh
	//
	ControlInfo* eb = controls.find(idc_control);

	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;
	box.vmin.y = 0;
	box.vmax.y = 0;

	bool b = bvh.del(box, idc_control);
	geAssert(b);

	//
	// move in avl
	//
	eb->x0 += dx;
	eb->y0 += dy;
	eb->x1 += dx;
	eb->y1 += dy;

	//
	// add back to bvh
	//
	box.vmin.x = (float)eb->x0;
	box.vmax.x = (float)eb->x1;
	box.vmin.z = (float)eb->y0;
	box.vmax.z = (float)eb->y1;

	bvh_type::node_pointer_type node = bvh.add(box);
	node->t = idc_control;
}

//this dumps all box at (z=0)
void CNIFStaticBuffer::dumpBox(CGrowableArrayWithLast< GE_MATH::CAabbBox4D>& boxes)
{
	tree_type::iterator iter;
	controls.set_iterator_to_first(iter);
	for (ControlInfo* ci = controls.get_current_value(iter); ci; ci = controls.get_next_value(iter)) {
		unsigned int idc_control = *controls.get_current_key(iter);
		GE_MATH::CAabbBox4D& box = boxes.append();

		int x0 = ci->x0;
		int y0 = ci->y0;
		int x1 = ci->x1;
		int y1 = ci->y1;

		x0 -= screenW >> 1;			//from left(0) to center(0)
		x1 -= screenW >> 1;
		y0 = screenH - y0;			//from bottom(0) to top(0), axis goes top-down
		y1 = screenH - y1;
		y0 -= screenH >> 1;			//from top(0) to center(0), axis goes top-down
		y1 -= screenH >> 1;

		//
		float pixelHeight = 2.f / screenH;
		float pixelWidth = 2.f / screenW;

		float yTop = y0 * pixelHeight;
		float yBot = y1 * pixelHeight;
		float xa = x0 * pixelWidth;
		float xb = x1 * pixelWidth;

		yTop -= pixelHeight * 0.5f;		//TODO: is this shifting really still needed??? (microsoft map texel to pixel)
		yBot -= pixelHeight * 0.5f;
		xa -= pixelWidth * 0.5f;
		xb -= pixelWidth * 0.5f;

		box.vmin.set(xa, yBot, 0); 
		box.vmax.set(xb, yTop, 0);
	}
}

void CNIFStaticBuffer::dump(void)
{
	//CNIFBase::dump(vbPos);
	ODS("vbPos[%d]\n", vbPos.getData().get_count());
	ODS("vbUv[%d]\n", vbUv.getData().get_count());
	
	tex.dump();
	CNIFColorIdx::dump();


}

CNIFStaticBuffer::ControlInfo* CNIFStaticBuffer::avlFind(unsigned int idc_control)
{
	return controls.find(idc_control);
}

bool CNIFStaticBuffer::bvhDel(GE_MATH::CAabbBox4D& box, unsigned int id)
{
	return bvh.del(box, id);
}

CNIFStaticBuffer::bvh_type::Node* CNIFStaticBuffer::bvhAdd(GE_MATH::CAabbBox4D& box)
{
	return bvh.add(box);
}

unsigned int CNIFStaticBuffer::bvhFind(int x, int y)
{
	query_result.cleanup_content();

	GE_MATH::CAabbBox4D box;
	box.vmin.x = (float)x;
	box.vmax.x = (float)x;
	box.vmin.z = (float)y;
	box.vmax.z = (float)y;

	bvh.queryBoxXZ(box, query_result);

	if (query_result.get_count()) {
		return query_result[0]->t;
	}

	return -1;
}

void CNIFStaticBuffer::getStringFromPool(unsigned int headNodeId, CGrowableArrayWithLast<wchar_t>& str)
{
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(headNodeId);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		str.push(node->t.c);
	}
	str.push(0);
}

void CNIFStaticBuffer::getStringColorsFromPool(unsigned int headNodeId, 
	CGrowableArrayWithLast<wchar_t>& str, CGrowableArrayWithLast<unsigned char>& colors)
{
	pool_type::HeadNode* headNode = pool.headNodeId2Ptr(headNodeId);
	for (pool_type::Node* node = pool.getFirstNode(headNode); node; node = pool.getNextNode(node)) {
		str.push(node->t.c);
		colors.push(node->t.colorIdx);
	}
	str.push(0);
}

void CNIFStaticBuffer::changeColorIdx(unsigned int idc_control, unsigned char colorIdx)
{
	//
	// find from avl
	//
	ControlInfo* info = controls.find(idc_control);
	geAssert(info);

	//definition of colorIdx is in install_nif_font()
	unsigned int pos = info->vbPosIdx << 2;
	for (unsigned int i = 0; i < 4; i++) {
		vbColorIdx.getData()[pos++].colorIdx = colorIdx;
	}
}

void CNIFStaticBuffer::setCallback(int idc_control, CB cb)
{
	ControlInfo* ci = avlFind(idc_control);
	geAssert(ci);

	ci->cb = cb;
}

void CNIFStaticBuffer::onrBtnDrag(int x, int y, int dx, int dy, int msg_type)
{
	unsigned int idc_control = bvhFind(x, y);
	if (lastRBtnDownId == -1 && idc_control == -1) {
		return;
	}

	switch (msg_type) {
	case MSG_RBTN_DOWN:
		lastRBtnDownId = idc_control;
		//ODS("down\n");
		break;
	case MSG_RBTN_UP:
		lastRBtnDownId = -1;
		//ODS("up\n");
		break;
	case MSG_MOUSE_OVER:
		if (lastRBtnDownId != -1) {
			moveAllControls(dx, dy);
			rebuildFromAvl(false);
			updateVbPosUvColor();
			//ODS("over\n");
		}
		break;
	}
}

void CNIFStaticBuffer::onlBtnClickCallbacks(int x, int y, int msg_type, 
	unsigned char colorLeave, unsigned char colorOver, unsigned char colorDown)
{
	unsigned int idc_control = bvhFind(x, y);

	//
	// leave (close pair)
	//
	if (idc_control == -1) {
		if (lastMouseOverId != -1) {
			ControlInfo* lastCi = avlFind(lastMouseOverId);
			if (lastCi->cb) {
				changeTextColor(lastCi, colorLeave);
				updateVbColorIdx();
			}
			lastMouseOverId = -1;
		}
		if (msg_type == MSG_LBTN_UP && lastLBtnDownId != -1) {
			ControlInfo* lastCi = avlFind(lastLBtnDownId);
			if (lastCi->cb) {
				changeTextColor(lastCi, colorLeave);
				updateVbColorIdx();
			}
			lastLBtnDownId = -1;
		}
		return;
	}

	//
	// (open pair) -> this means set downId, overId, etc, in //set status
	//
	ControlInfo* ci;

	unsigned int cnt = query_result.get_count();
	if (cnt > 1) {
		//
		//sort the result with respect to parent, then call the one with minZ, or maybe just use active one???
		//

		//
		//TODO: just find the one with callback, assuming no-overlapping.
		//
		for (unsigned int i = 0; i < cnt; i++) {
			idc_control = query_result[i]->t;
			ci = avlFind(idc_control);
			if (ci->cb) {
				break;
			}
		}

	}
	else {
		ci = avlFind(idc_control);
	}

	//
	// close pair for mouse_over
	//
	if (lastMouseOverId != -1 && lastMouseOverId != idc_control) {
		ControlInfo* lastCi = avlFind(lastMouseOverId);
		if (lastCi->cb) {
			changeTextColor(lastCi, colorLeave);
			updateVbColorIdx();
		}
	}

	//
	// call cb
	//
	if (ci && ci->cb) {
		switch (msg_type) {
		case MSG_LBTN_DOWN:
			changeTextColor(ci, colorDown);
			updateVbColorIdx();
			break;
		case MSG_LBTN_UP:
			if (lastLBtnDownId != idc_control) {
				changeTextColor(ci, colorLeave);
				updateVbColorIdx();
			}
			else {
				changeTextColor(idc_control, colorLeave);
				updateVbColorIdx();
				ci->cb(msg_type, this, idc_control, ci);
			}
			break;
		case MSG_MOUSE_OVER:
			changeTextColor(ci, colorOver);
			updateVbColorIdx();
			break;
		}
	}

	//
	// set status
	// 
	switch (msg_type) {
	case MSG_LBTN_DOWN:
		lastLBtnDownId = idc_control;
		break;
	case MSG_LBTN_UP:
		lastLBtnDownId = -1;
		break;
	}

	lastMouseOverId = idc_control;
}

void CNIFStaticBuffer::processCallbacks(int x, int y, int msg_type)
{
	unsigned int idc_control = bvhFind(x, y);

	//
	// leave (close pair)
	//
	if (idc_control == -1) {
		if (lastMouseOverId != -1) {
			ControlInfo* lastCi = avlFind(lastMouseOverId);
			if (lastCi->cb) {
				lastCi->cb(MSG_MOUSE_LEAVE, this, idc_control, lastCi);
			}
			lastMouseOverId = -1;
		}
		if (msg_type == MSG_LBTN_UP && lastLBtnDownId != -1) {
			ControlInfo* lastCi = avlFind(lastLBtnDownId);
			if (lastCi->cb) {
				lastCi->cb(MSG_LBTN_UP, this, idc_control, lastCi);
			}
			lastLBtnDownId = -1;
		}
		return;
	}

	//
	// (open pair) -> this means set downId, overId, etc, in //set status
	//
	ControlInfo* ci;

	unsigned int cnt = query_result.get_count();
	if (cnt > 1) {
		//
		//sort the result with respect to parent, then call the one with minZ, or maybe just use active one???
		//

		//
		//TODO: just find the one with callback, assuming no-overlapping.
		//
		for (unsigned int i = 0; i < cnt; i++) {
			idc_control = query_result[i]->t;
			ci = avlFind(idc_control);
			if (ci->cb) {
				break;
			}
		}

	}
	else {
		ci = avlFind(idc_control);
	}

	//
	// close pair for mouse_over
	//
	if (lastMouseOverId != -1 && lastMouseOverId != idc_control) {
		ControlInfo* lastCi = avlFind(lastMouseOverId);
		if (lastCi->cb) {
			lastCi->cb(MSG_MOUSE_LEAVE, this, lastMouseOverId, lastCi);
		}
	}

	//
	// call cb
	//
	if (ci && ci->cb) {
		ci->cb(msg_type, this, idc_control, ci);
	}

	//
	// set status
	// 
	switch (msg_type) {
	case MSG_LBTN_DOWN:
		lastLBtnDownId = idc_control;
		break;
	case MSG_LBTN_UP:
		lastLBtnDownId = -1;
		break;
	}

	lastMouseOverId = idc_control;
}

// after adding and deleting, this rebuild the entire vbPos, vbUv and vbColorIdx.
//1, render oder: small id first in controls.
void CNIFStaticBuffer::rebuildFromAvl(bool createFont)
{
	//
	//
	//
	vbPosIdx = 0;

	vbPos.reset();
	vbUv.reset();
	alignIds.cleanup_content();

	vbColorIdx.reset();

	if (createFont) {
		tex.reset();
	}

	CGrowableArrayWithLast<wchar_t> str;
	CGrowableArrayWithLast<unsigned char>colors;

	tree_type::iterator iter;
	controls.set_iterator_to_first(iter);
	for (ControlInfo* eb = controls.get_current_value(iter); eb; eb = controls.get_next_value(iter)) {
		unsigned int idc_control = *controls.get_current_key(iter);

		int added, totalLines;
	
		switch (eb->type) {
		case 0:
			str.cleanup_content();
			colors.cleanup_content();
			getStringColorsFromPool(eb->strHeadNode, str, colors);
			addText(idc_control, eb->align, eb->localAlign, str.get_objects(), colors.get_objects(),
				eb->x0 - alignOrigs[eb->align].x, 
				eb->x1 - alignOrigs[eb->align].x,
				eb->y0 - alignOrigs[eb->align].y,
				eb->y1 - alignOrigs[eb->align].y, false, createFont, wcslen(str.get_objects()), added, false, totalLines);
			break;
		case 1:
			//geBreak(idc_control == 13);
			addImage(idc_control, eb->align, eb->slice, eb->colorIdx, eb->ltu, eb->ltv, eb->bru, eb->brv, 
				eb->x0 - alignOrigs[eb->align].x,
				eb->x1 - alignOrigs[eb->align].x,
				eb->y0 - alignOrigs[eb->align].y,
				eb->y1 - alignOrigs[eb->align].y, false, -1);
			break;
		default:
			geBreak(1);
			break;
		}
	}
}








//1, differences: display grid and pixels -> grid start from 0, pixels start from 0.5
//2, The edges of the input quad need to lie upon the boundary lines between pixel cells. By simply shifting the x and y quad coordinates by -0.5 units, 
//	texel cells will perfectly cover pixel cells and the quad can be perfectly recreated on the screen
//3***, picture a 2x2 screen mapped with 2x2 texture quad, the pixels/texels are located at each individual center of screen cells.
//4******-> screen cell centers, pixels, texels, those THREE things are at the same place. (pixel/texel are all infinitely small points, screen cells are 
//	not, they are squares), and no such thing as texel quad??  
//5, all above are for how to rasterize, with all those anti-aliasing technique or how to precisely(no streches) put a texture on screen.
