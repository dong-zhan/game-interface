#ifndef _INCLUDE_NIF_BASE_H_
#define _INCLUDE_NIF_BASE_H_

#include "ge_util/hashlist.h"
#include "app11/app11fontbitmap.h"
#include "app11/app11dynamicbuffer.h"

#include "sp/sp_dynbvh.h"
#include "ge_util/poollist.h"

#if 0
//adjust all the vertices to correctly line up texels with pixels 
//texel is in texture space -> (0, texture size-1)
inline float texelToScreen(float x) { return x - 0.5f; }

//texel and pixel are all points at cell center, screen coordinates start from cell border.
inline float pixelToNdcX(float texelSize, unsigned int screenPos){
	return (texelToScreen((float)screenPos)) * texelSize * 2 - 1.f;
}

inline float pixelToNdcY(float texelSize, unsigned int screenPos) {
	return 1.f - (texelToScreen((float)screenPos)) * texelSize * 2;
}
#endif


/////////////////////////////////////////////////////////////////////////////////
//				CNIFFontBitmap
//
//1, this copies text from dynBitmap to tex, so, updating of the dynBitmap won't affect text cache here.
class CNIFFontBitmap
{
public:
	struct CHAR_INFO {
		CApp11FontBitmap::char_info info;		//this is info for cells in local bitmap(tex), cooresponding to the one in dynBitmap.
		unsigned int ref_count;		//for dynamic bitmap
	};
	typedef CHashList<wchar_t, CHAR_INFO, unsigned short> infos_type;
	struct FreeCell {
		unsigned int row, col;
	};

protected:

	CGrowableArrayWithLast<FreeCell>freeList;
	CApp11FontBitmapDynamic* dynBitmap;
	CDX11Texture2D tex;
	CDX11ShaderResourceView srv;
	float texelWidth;
	float texelHeight;
	infos_type infos;
	unsigned int max_row, max_col, current_row, current_col;

public:
	void reset(void);

	CApp11FontBitmapDynamic* getDynBitmap(void) { return dynBitmap; }
	CDX11Texture2D& getTex(void) { return tex; }
	CDX11ShaderResourceView& getSrv(void) { return srv; }
	CNIFFontBitmap(CApp11FontBitmapDynamic* dynBitmap) : dynBitmap(dynBitmap) {}
	CNIFFontBitmap() {}
	void init(CApp11FontBitmapDynamic* dynBitmap) { this->dynBitmap = dynBitmap; }
	bool create(int cellHeight, int capacity);
	void getCharW(int x, int w, float& x0, float& x1);
	void getCharH(int y, int h, float& y0, float& y1);

	bool delChar(wchar_t c);


	unsigned int infosPtr2Id(infos_type::Node*node);
	infos_type::Node* infosId2Ptr(unsigned int id);
	infos_type::Node* find(wchar_t c);
	infos_type::Node* convert(wchar_t c, CApp11FontBitmap::char_info& info);

public:
	void dump(void);
};

/////////////////////////////////////////////////////////////////////////////////
//					CFixedSizedQuadIb
//
class CFixedSizedQuadIb
{
protected:
	CDX11Buffer ib;
public:
	bool create(int quadCount);
	CDX11Buffer& getIb(void) { return ib; }
};

/////////////////////////////////////////////////////////////////////////////////
//					CNIFBase
//
class CNIFBase
{
public:
	struct AlignDiff {
		int dx;
		int dy;
	};
	struct Orig {
		int x;		
		int y;		
	};

protected:
	int screenW, screenH;

	//another way to deal with tex is to clear all tex, then rebuild everything if ref_count is not a desired way.
	CNIFFontBitmap tex;

	AlignDiff alignDiffs[9];
	Orig alignOrigs[9];

	CFixedSizedQuadIb* ib;

public:
	int getScreenW(void) { return screenW; }
	int getScreenH(void) { return screenH; }
	Orig* getAlignOrigs(void) { return alignOrigs; }
	AlignDiff* getAlignDiffs(void) { return alignDiffs; }

	CNIFFontBitmap& getTex(void) { return tex; }
	void init(CApp11FontBitmapDynamic* dynBitmap, CFixedSizedQuadIb* ib, int cellHeight, int capacity, int screenW, int screenH);

public:
	int getXOriginFromAlign(int align);
	int getYOriginFromAlign(int align);

	void calculateAlignsDiff(int screenW, int screenH);
	void calculateAlignsOrig(int screenW, int screenH);

public:
	void dump(CApp11DynamicBuffer<CVector2>& vbPos);
};

/////////////////////////////////////////////////////////////////////////////////
//					CNIFColorIdx
//
class CNIFColorIdx
{
public:
	struct Indices {
		unsigned char colorIdx, slice;
		unsigned char pad[2];
	};

protected:
	CApp11DynamicBuffer<Indices> vbColorIdx;

public:
	CNIFColorIdx() : vbColorIdx(CApp11DynamicBuffer<Indices>::vb_type) {}

public:
	void setAllTextColor(unsigned char colorIdx, unsigned int cnt);		//this is for font, indices.slice = 0xff;
	void pushColorIdxSlice(unsigned char colorIdx, unsigned char slice);
	void setColorIdxSlice(unsigned int idx, unsigned char colorIdx, unsigned char slice);

public:
	void updateVbColorIdx(void);

public:
	void dump(void);
};

/////////////////////////////////////////////////////////////////////////////////
//					CNIFDynamicBuffer
//
class CNIFDynamicBuffer : public CNIFBase, public CNIFColorIdx
{
public:
	struct EditBox {
		int x0, x1, y0, y1;
		unsigned int strHeadNode;
		unsigned short alignId;
	};

	struct Char {
		wchar_t c;			//every c has a unique id.
		unsigned short id;			//id in CNIFBase::tex for char info, especially w
	};
	
	typedef GE_SP::CDynBvh<unsigned int>bvh_type;		//key is IDC_EDITBOX
	typedef CDoubleLinkedListPool<Char, unsigned short> pool_type;			//for texts
	typedef CAVL_Tree<unsigned int, EditBox> tree_type;	//key is IDC_EDITBOX

public:
	EditBox* findEditBox(unsigned int id);		//for test
	
protected:
	int caretPos;
	EditBox* caretEditbox;		//the editbox where caret is in.
	CNIFFontBitmap::CHAR_INFO* caretInfo;		//it's safe to use this pointer.

public:
	void setCaret(int x, int y);
	int getCharPosInText(EditBox* text, int pos);


protected:
	CApp11DynamicBuffer<CVector2> vbPos;
	CApp11DynamicBuffer<CVector2> vbUv;

	bvh_type bvh;		//for search
	pool_type pool;		//store editbox string, and a pointer to char info
	tree_type editboxes;		//all edit boxes.

	bvh_type::query_result_type query_result;
	void getStringColorsFromPool(unsigned int headNodeId, CGrowableArrayWithLast<wchar_t>& str);

public:
	CNIFDynamicBuffer() : vbPos(CApp11DynamicBuffer<CVector2>::vb_type),
		vbUv(CApp11DynamicBuffer<CVector2>::vb_type){}

	void updateVb(void);

	void init(CApp11FontBitmapDynamic* dynBitmap, CFixedSizedQuadIb* ib, int cellHeight, int capacity, int screenW, int screenH);

	void addEditBox(unsigned int idc_editbox, unsigned int alignId, int x0, int x1, int y0, int y1);	//input coordinates is range
	void moveEditBox(unsigned int idc_editbox, int dx, int dy);

	bool on_char_backspace(void);
	bool on_char_del(void);
	bool on_char(WPARAM wParam, LPARAM lParam);

public:
	void advanceCaret(void);
	void updateCaretPosUv(EditBox* caretEditbox, int caretPos);
	void updateEditboxesData(void);
	void updateEditboxesData(EditBox* caretEditbox, float pixelWidth, float pixelHeight, float hpw, float hph);
	void render(void);
	void resize(int screenW, int screenH);

public:
	void insert(EditBox* eb, wchar_t c, int pos);
	bool del(EditBox* eb, int pos);
	bool del(unsigned int idc_editbox);

public:
	void updateAll(void);

public:
	void dump(void);
};


/////////////////////////////////////////////////////////////////////////////////
//					CNIFStaticBuffer
//
//1, moving icon can be done in 3 steps -> a, remove icon, b, use another shader to render moving oject, c, drop to add back.
//2, *** if order is needed, there must be an order tree, add/rebuild according to order tree, but, i think, use idc_control as order is enough.
//	what's the point creating a lot of overlaps in one dialog?
class CNIFStaticBuffer : public CNIFBase, public CNIFColorIdx
{
public:
	struct ControlInfo;
	typedef void(*CB)(int msg_type, CNIFStaticBuffer* sb, unsigned int idc_control, ControlInfo* ci);  

	struct ControlInfo {
		unsigned char type;
		int x0, x1, y0, y1;
		float ltu, ltv, bru, brv;

		unsigned int strHeadNode;
		char align;		//0-8, index is faster than (alignX, alignY), relative to window
		char localAlign;			//relative to control box(x0,x1,y0,y1)
		unsigned char slice;
		unsigned char colorIdx;		//this is for image
		unsigned int vbPosIdx;
		CB cb;
		ControlInfo() : cb(NULL), localAlign(-1){}
	};

	struct Char {
		wchar_t c;					//every c has a unique id.
		unsigned short id;			//id in CNIFBase::tex for char info, especially w
		unsigned char colorIdx;		//this is for text/char
	};

	typedef GE_SP::CDynBvh<unsigned int>bvh_type;		//key is IDC_EDITBOX
	typedef CDoubleLinkedListPool<Char, unsigned short> pool_type;			//for texts
	typedef CAVL_Tree<unsigned int, ControlInfo> tree_type;	//key is IDC_EDITBOX

private:
	unsigned int vbPosIdx;		

protected:
	bvh_type bvh;		//for search
	pool_type pool;		//store editbox string, and a pointer to char info
	tree_type controls;

	bvh_type::query_result_type query_result;

protected:		//buffers and alignIds
	CApp11DynamicBuffer<CVector2> vbPos;
	CApp11DynamicBuffer<CVector2> vbUv;
	//CApp11DynamicBuffer<Indices> vbColorIdx;   //this has been moved to CNIFColorIdx

	//about parent: create a parent list, assign all square to parent, when moving parent, all children move along with it.
	//parent can be an dummy object or have an 'background' image to it.
	//CGrowableArrayWithLast<unsigned short>parents;

	CGrowableArrayWithLast<unsigned int>alignIds;

public:
	pool_type& getPool(void) { return pool; }
	CApp11DynamicBuffer<CVector2>& getVbUv(void) { return vbUv; }
	CApp11DynamicBuffer<CVector2>& getVbPos(void) { return vbPos; }
	CNIFStaticBuffer() : vbPos(CApp11DynamicBuffer<CVector2>::vb_type),
		vbUv(CApp11DynamicBuffer<CVector2>::vb_type), vbPosIdx(0), lastMouseOverId(-1), lastLBtnDownId(-1), lastRBtnDownId(-1){}

	//
	// basic functions
	//
	bool addText(unsigned int idc_control, unsigned int alignId, char localAlign, wchar_t* str, unsigned char* colors, 
		int x0, int x1, int y0, int y1, bool addToAvlBvh, bool createFont, int strLen, int& added, bool autoWrap, int& totalLines);  //input coordinates is range
	void addImage(unsigned int idc_control, unsigned int alignId, unsigned char slice, unsigned char colorIdx,
		float ltu, float ltv, float bru, float brv, 
		int x0, int x1, int y0, int y1, bool addToAvlBvh, unsigned int reservedVbPos); //input coordinates is range
	void addSquardIcon(unsigned int idc_control, unsigned int alignId, unsigned char slice, unsigned char colorIdx,
		int imageW, int iconW, int iconx, int icony, int x, int y, bool addToAvlBvh);
	void rebuildFromAvl(bool createFont);
	void moveControl(unsigned int idc_control, int dx, int dy);
	void moveAllControlsInBvh(bvh_type::Node*node, int dx, int dy);
	void moveAllControls(int dx, int dy);
	unsigned int reserveQuad(void);

	//
	// string/text functions
	//
	void addText(pool_type::HeadNode* headNode, wchar_t* str, unsigned char* colors, int strLen);
	bool replaceText(ControlInfo* ci, wchar_t* str, unsigned char* colors, int strLen);
	void delText(unsigned int strHeadNodeId);
	void getStringColorsFromPool(unsigned int headNodeId, CGrowableArrayWithLast<wchar_t>& str, CGrowableArrayWithLast<unsigned char>& colors);
	void getStringFromPool(unsigned int headNodeId, CGrowableArrayWithLast<wchar_t>& str);

	//
	// misc
	//
	bool changeTextColor(ControlInfo* ci, unsigned char* colors, unsigned int cnt);
	bool changeTextColor(ControlInfo* ci, unsigned char colorIdx);

	bool changeTextColor(unsigned int idc_control, unsigned char* colors, unsigned int cnt);
	bool changeTextColor(unsigned int idc_control, unsigned char colorIdx);
	void changeColorIdx(unsigned int idc_control, unsigned char colorIdx);
	void alignText(ControlInfo* ci, int oldAlignedId, int newAlignId);
	int getTextLenInPixel(ControlInfo* ci, int&cnt);
	void shiftTextX(ControlInfo* ci, int cnt, float dx);

	//
	// deletion
	//
	bool del(unsigned int idc_control);

	//
	// render
	//
	void render(void);
	void resize(int screenW, int screenH);

	//
	// update
	//
	void updateVbPos(void);
	void updateVbPosUvColor(void);

	// bvh
	unsigned int bvhFind(int x, int y);
	bool bvhDel(GE_MATH::CAabbBox4D& box, unsigned int id);
	bvh_type::Node* bvhAdd(GE_MATH::CAabbBox4D& box);
	bvh_type::query_result_type& getQueryResult(void) { return query_result; }

	//avl
	ControlInfo* avlFind(unsigned int idc_control);


protected:
	unsigned int lastMouseOverId, lastLBtnDownId, lastRBtnDownId;

public:
	//
	// mouse messages 
	//
	//up down pair: a down must be paired with a up for a control
	static const int MSG_LBTN_DOWN = 0;
	static const int MSG_LBTN_UP = 1;

	//leave over pair: multiple over, but one leave
	static const int MSG_MOUSE_LEAVE = -1;
	static const int MSG_MOUSE_OVER = 2;

	//
	static const int MSG_RBTN_DOWN = 3;
	static const int MSG_RBTN_UP = 4;


	//
	// callbacks
	//
	unsigned int getLastMouseOverId(void) { return lastMouseOverId; }
	unsigned int getLastLBtnDownId(void) { return lastLBtnDownId; }
	unsigned int getLastRBtnDownId(void) { return lastRBtnDownId; }
	void processCallbacks(int x, int y, int msg_type);
	void onlBtnClickCallbacks(int x, int y, int msg_type, unsigned char colorLeave, unsigned char colorOver, unsigned char colorDown);
	void onrBtnDrag(int x, int y, int dx, int dy, int msg_type);
	void setCallback(int idc_control, CB cb);

public: //debug
	void dump(void);
	void dumpBox(CGrowableArrayWithLast< GE_MATH::CAabbBox4D>& boxes);
};




#endif
