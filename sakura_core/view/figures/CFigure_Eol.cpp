﻿/*! @file */
/*
	Copyright (C) 2008, kobake
	Copyright (C) 2018-2021, Sakura Editor Organization

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CFigure_Eol.h"
#include "types/CTypeSupport.h"
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include "window/CEditWnd.h"

//EOF描画関数
//実際には pX と nX が更新される。
//2004.05.29 genta
//2007.08.25 kobake 戻り値を void に変更。引数 x, y を DispPos に変更
//2007.08.25 kobake 引数から nCharWidth, nLineHeight を削除
//2007.08.28 kobake 引数 fuOptions を削除
//void _DispEOF( CGraphics& gr, DispPos* pDispPos, const CEditView* pcView, bool bTrans);

//改行記号描画
//2007.08.30 kobake 追加
void _DispEOL(CGraphics& gr, DispPos* pDispPos, CEol cEol, const CEditView* pcView, bool bTrans, HPEN hPen);

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                        CFigure_Eol                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CFigure_Eol::Match(const wchar_t* pText, int nTextLen) const
{
	// 2014.06.18 折り返し・最終行だとDrawImpでcEol.GetLen()==0になり無限ループするので
	// もしも行の途中に改行コードがあった場合はMatchさせない
	if(nTextLen == 2 && pText[0]==L'\r' && pText[1]==L'\n')return true;
	if(nTextLen == 1 && WCODE::IsLineDelimiterExt(pText[0]))return true;
	return false;
}

inline const ColorInfo& GetColorInfo(const STypeConfig* p, int nColorIndex)
{
	return p->m_ColorInfoArr[ToColorInfoArrIndex(EColorIndexType(nColorIndex))];
}

// 2006.04.29 Moca 選択処理のため縦線処理を追加
//$$ 高速化可能。
bool CFigure_Eol::DrawImp(SColorStrategyInfo* pInfo)
{
	CEditView* pcView = pInfo->m_pcView;

	// 改行取得
	const CLayout* pcLayout = pInfo->m_pDispPos->GetLayoutRef();
	CEol cEol = pcLayout->GetLayoutEol();
	if(cEol.GetLen()){
		
		// CFigureSpace::DrawImp_StyleSelectもどき。選択・検索色を優先する
		const ColorInfoBase& colorType = GetColorInfo(m_pTypeData, pInfo->GetCurrentColor());	// 周辺の色（現在の指定色/選択色）
		const ColorInfoBase& colorType2 = GetColorInfo(m_pTypeData, pInfo->GetCurrentColor2());	// 周辺の色（現在の指定色）
		const ColorInfoBase& colorText = GetColorInfo(m_pTypeData, COLORIDX_TEXT);				// テキストの指定色
		const ColorInfoBase& colorSpace = GetColorInfo(m_pTypeData, GetDispColorIdx());	// 空白の指定色
		const ColorInfoBase& colorBg = GetColorInfo(m_pTypeData, pInfo->GetCurrentColorBg());
		const COLORREF crTextBack = colorText.GetBackColor();

		ColorInfoBase colorType3 = SetColorInfoFromMarker(pInfo, colorType2, ToColorInfoArrIndex(pInfo->GetCurrentColor2()));
		const ColorInfoBase& colorType4 = (colorType3.GetBackColor() == crTextBack ? colorBg: colorType3);
		const ColorInfoBase* pcSearch = NULL;	// 検索色(EOL固有)
		COLORREF crText;
		COLORREF crBack;
		const bool bNotSelecting = pInfo->GetCurrentColor() == pInfo->GetCurrentColor2();
		const bool blendColor = !bNotSelecting && colorType.GetTextColor() == colorType.GetBackColor(); // 選択混合色
		bool bBold;
		const ColorInfoBase *pcText, *pcBack;
		if( !bNotSelecting && !blendColor ){
			// 指定選択色:選択色を優先
			pcText = &colorType;
			pcBack = (colorType.GetBackColor() == crTextBack ? &colorBg: &colorType); // colorType5
			bBold = colorType.IsBoldFont();
		}else if( COLORIDX_SEARCH <= pInfo->GetCurrentColor2() && pInfo->GetCurrentColor2() <= COLORIDX_SEARCHTAIL ){
			// 検索and通常,検索andアルファ:検索色優先
			pcSearch = &GetColorInfo(m_pTypeData, pInfo->GetCurrentColor2());
			pcText = pcSearch;
			pcBack = (pcSearch->GetBackColor() == crTextBack ? &colorBg: pcSearch);
			bBold = pcSearch->IsBoldFont();
		}else{
			// アルファor通常:改行の色優先
			const bool bText = colorSpace.GetTextColor() == colorText.GetTextColor();
			const bool bBack = colorSpace.GetBackColor() == crTextBack;
			pcText = (bText ? &colorType3 : &colorSpace);
			pcBack = (bBack ? &colorType4 : &colorSpace);
			bBold = colorType3.IsBoldFont();
		}
		if( blendColor ){
			// 混合色(検索色を優先しつつ)
			crText = pcView->GetTextColorByColorInfo2(colorType, *pcText);
			crBack = pcView->GetBackColorByColorInfo2(colorType, *pcBack);
		}else{
			crText = pcText->GetTextColor();
			crBack = pcBack->GetBackColor();
		}
		pInfo->m_gr.PushTextForeColor(crText);
		pInfo->m_gr.PushTextBackColor(crBack);
		bool bTrans = pcView->IsBkBitmap() && colorText.GetBackColor() == crBack;
		SFONT sFont;
		sFont.m_sFontAttr.m_bBoldFont = colorSpace.IsBoldFont() || bBold;
		sFont.m_sFontAttr.m_bUnderLine = colorSpace.HasUnderLine();
		sFont.m_hFont = pInfo->m_pcView->GetFontset().ChooseFontHandle( 0, sFont.m_sFontAttr );
		pInfo->m_gr.PushMyFont(sFont);

		DispPos sPos(*pInfo->m_pDispPos);	// 現在位置を覚えておく

		if (crText != m_clrPen || m_hPen == NULL) {
			if (m_hPen != NULL) {
				::DeleteObject(m_hPen);
			}
			m_hPen = CreatePen(PS_SOLID, 1, crText);
			m_clrPen = crText;
		}

		_DispEOL(pInfo->m_gr, pInfo->m_pDispPos, cEol, pcView, bTrans, m_hPen);
		DrawImp_StylePop(pInfo);
		DrawImp_DrawUnderline(pInfo, sPos);

		pInfo->m_nPosInLogic+=cEol.GetLen();
	}else{
		// 無限ループ対策
		pInfo->m_nPosInLogic += 1;
		assert_warning( 1 );
	}

	return true;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     折り返し描画実装                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

// 折り返し描画
bool CFigure_Wrap::DrawImp(SColorStrategyInfo* pInfo){
	// 1文字進んでしまうので戻す
	CLogicXInt posBk = pInfo->m_nPosInLogic;
	bool ret = CFigureSpace::DrawImp(pInfo);
	pInfo->m_nPosInLogic = posBk;
	return ret;
}

void CFigure_Wrap::DispSpace(CGraphics& gr, DispPos* pDispPos, CEditView* pcView, bool bTrans) const
{
	const wchar_t* szText;
	CTypeSupport cWrapType(pcView, this->GetColorIdx());
	if (cWrapType.IsDisp()) {
		szText = L"<";
	}else{
		szText = L" ";
	}
	CLayoutXInt width = CLayoutXInt(pcView->GetTextMetrics().CalcTextWidth3(szText, 1));
	RECT rcClip2;
	if(pcView->GetTextArea().GenerateClipRect(&rcClip2, *pDispPos, width))
	{
		int fontNo = WCODE::GetFontNo(*szText);
		int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
		int nDx[1] = {(Int)width};

		//描画
		::ExtTextOut(
			gr,
			pDispPos->GetDrawPos().x,
			pDispPos->GetDrawPos().y + nHeightMargin,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClip2,
			szText,
			wcslen(szText),
			nDx
		);
	}
	pDispPos->ForwardDrawCol(width);
}
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       EOF描画実装                           //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
/*!
EOF記号の描画
@date 2004.05.29 genta  MIKさんのアドバイスにより関数にくくりだし
@date 2007.08.28 kobake 引数 nCharWidth 削除
@date 2007.08.28 kobake 引数 fuOptions 削除
@date 2007.08.30 kobake 引数 EofColInfo 削除
*/
void _DispEOF(
	CGraphics&			gr,			//!< [in] 描画対象のDevice Context
	DispPos*			pDispPos,	//!< [in] 表示座標
	const CEditView*	pcView
)
{
	// 描画に使う色情報
	CTypeSupport cEofType(pcView,COLORIDX_EOF);
	if(!cEofType.IsDisp())
		return;
	CTypeSupport cTextType(pcView,COLORIDX_TEXT);
	bool bTrans = pcView->IsBkBitmap() && cEofType.GetBackColor() == cTextType.GetBackColor();

	//必要なインターフェースを取得
	const CTextMetrics* pMetrics=&pcView->GetTextMetrics();
	const CTextArea* pArea=&pcView->GetTextArea();

	//定数
	static const wchar_t	szEof[] = L"[EOF]";
	const int		nEofLen = _countof(szEof) - 1;

	cEofType.SetGraphicsState_WhileThisObj(gr);
	int fontNo = WCODE::GetFontNo('E');
	int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
	pcView->GetTextDrawer().DispText(gr, pDispPos, nHeightMargin, szEof, nEofLen, bTrans);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       改行描画実装                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

//画面描画補助関数
//May 23, 2000 genta
//@@@ 2001.12.21 YAZAKI 改行記号の書きかたが変だったので修正
void _DrawEOL(
	CGraphics&		gr,
	const CMyRect&	rcEol,
	CEol			cEol,
	bool			bBold,
	HPEN			hPen
);

//2007.08.30 kobake 追加
void _DispEOL(CGraphics& gr, DispPos* pDispPos, CEol cEol, const CEditView* pcView, bool bTrans, HPEN hPen)
{
	const CLayoutXInt nCol = CTypeSupport(pcView,COLORIDX_EOL).IsDisp()
		? pcView->GetTextMetrics().GetLayoutXDefault(CKetaXInt(1)) + CLayoutXInt(4) // ONのときは1幅+4px
		: CLayoutXInt(2); // HACK:EOL off なら2px
	RECT rcClip2;
	if(pcView->GetTextArea().GenerateClipRect(&rcClip2,*pDispPos,nCol)){
		int fontNo = WCODE::GetFontNo(' ');
		int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
		// 2003.08.17 ryoji 改行文字が欠けないように
		::ExtTextOut(
			gr,
			pDispPos->GetDrawPos().x,
			pDispPos->GetDrawPos().y + nHeightMargin,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClip2,
			L"  ",
			2,
			pcView->GetTextMetrics().GetDxArray_AllHankaku()
		);

		// 改行記号の表示
		if( CTypeSupport(pcView,COLORIDX_EOL).IsDisp() ){
			// From Here 2003.08.17 ryoji 改行文字が欠けないように

			// リージョン作成、選択。
			gr.PushClipping(rcClip2);
			
			// 描画領域
			CMyRect rcEol;
			rcEol.SetPos(pDispPos->GetDrawPos().x + 1, pDispPos->GetDrawPos().y);
			rcEol.SetSize(pcView->GetTextMetrics().GetHankakuWidth(), pcView->GetTextMetrics().GetHankakuHeight());

			// 描画
			// 文字色や太字かどうかを現在の DC から調べる	// 2009.05.29 ryoji 
			// （検索マッチ等の状況に柔軟に対応するため、ここは記号の色指定には決め打ちしない）
			// 2013.06.21 novice 文字色、太字をCGraphicsから取得
			_DrawEOL(gr, rcEol, cEol, gr.GetCurrentMyFontBold(), hPen);

			// リージョン破棄
			gr.PopClipping();

			// To Here 2003.08.17 ryoji 改行文字が欠けないように
		}
	}

	//描画位置を進める(2桁)
	pDispPos->ForwardDrawCol(nCol);
}

//	May 23, 2000 genta
/*!
画面描画補助関数:
行末の改行マークを改行コードによって書き分ける（メイン）

@note bBoldがTRUEの時は横に1ドットずらして重ね書きを行うが、
あまり太く見えない。

@date 2001.12.21 YAZAKI 改行記号の描きかたを変更。ペンはこの関数内で作るようにした。
						矢印の先頭を、sx, syにして描画ルーチン書き直し。
*/
void _DrawEOL(
	CGraphics&		gr,		//!< Device Context Handle
	const CMyRect&	rcEol,		//!< 描画領域
	CEol			cEol,		//!< 行末コード種別
	bool			bBold,		//!< TRUE: 太字
	HPEN			hPen		//!< ペン
)
{
	int sx, sy;	//	矢印の先頭
	HPEN hPenOld = (HPEN)SelectObject(gr, hPen);

	switch( cEol.GetType() ){
	case EOL_CRLF:	//	下左矢印
		{
			sx = rcEol.left;						//X左端
			sy = rcEol.top + ( rcEol.Height() / 2);	//Y中心
			DWORD pp[] = { 3, 3 };
			POINT pt[6];
			pt[0].x = sx + rcEol.Width();	//	上へ
			pt[0].y = sy - rcEol.Height() / 4;
			pt[1].x = sx + rcEol.Width();	//	下へ
			pt[1].y = sy;
			pt[2].x = sx;	//	先頭へ
			pt[2].y = sy;
			pt[3].x = sx + rcEol.Height() / 4;	//	先頭から下へ
			pt[3].y = sy + rcEol.Height() / 4;
			pt[4].x = sx;	//	先頭へ戻り
			pt[4].y = sy;
			pt[5].x = sx + rcEol.Height() / 4;	//	先頭から上へ
			pt[5].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 1;	//	上へ（右へずらす）
				pt[0].y += 0;
				pt[1].x += 1;	//	右へ（右にひとつずれている）
				pt[1].y += 1;
				pt[2].x += 0;	//	先頭へ
				pt[2].y += 1;
				pt[3].x += 0;	//	先頭から下へ
				pt[3].y += 1;
				pt[4].x += 0;	//	先頭へ戻り
				pt[4].y += 1;
				pt[5].x += 0;	//	先頭から上へ
				pt[5].y += 1;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_CR:	//	左向き矢印	// 2007.08.17 ryoji EOL_LF -> EOL_CR
		{
			sx = rcEol.left;
			sy = rcEol.top + ( rcEol.Height() / 2 );
			DWORD pp[] = { 3, 2 };
			POINT pt[5];
			pt[0].x = sx + rcEol.Width();	//	右へ
			pt[0].y = sy;
			pt[1].x = sx;	//	先頭へ
			pt[1].y = sy;
			pt[2].x = sx + rcEol.Height() / 4;	//	先頭から下へ
			pt[2].y = sy + rcEol.Height() / 4;
			pt[3].x = sx;	//	先頭へ戻り
			pt[3].y = sy;
			pt[4].x = sx + rcEol.Height() / 4;	//	先頭から上へ
			pt[4].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 0;	//	右へ
				pt[0].y += 1;
				pt[1].x += 0;	//	先頭へ
				pt[1].y += 1;
				pt[2].x += 0;	//	先頭から下へ
				pt[2].y += 1;
				pt[3].x += 0;	//	先頭へ戻り
				pt[3].y += 1;
				pt[4].x += 0;	//	先頭から上へ
				pt[4].y += 1;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_LF:	//	下向き矢印	// 2007.08.17 ryoji EOL_CR -> EOL_LF
	// 2013.04.22 Moca NEL,LS,PS対応。暫定でLFと同じにする
		{
			sx = rcEol.left + ( rcEol.Width() / 2 );
			sy = rcEol.top + ( rcEol.Height() * 3 / 4 );
			DWORD pp[] = { 3, 2 };
			POINT pt[5];
			pt[0].x = sx;	//	上へ
			pt[0].y = rcEol.top + rcEol.Height() / 4 + 1;
			pt[1].x = sx;	//	上から下へ
			pt[1].y = sy;
			pt[2].x = sx - rcEol.Height() / 4;	//	そのまま左上へ
			pt[2].y = sy - rcEol.Height() / 4;
			pt[3].x = sx;	//	矢印の先端に戻る
			pt[3].y = sy;
			pt[4].x = sx + rcEol.Height() / 4;	//	そして右上へ
			pt[4].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if( bBold ){
				pt[0].x += 1;	//	上へ
				pt[0].y += 0;
				pt[1].x += 1;	//	上から下へ
				pt[1].y += 0;
				pt[2].x += 1;	//	そのまま左上へ
				pt[2].y += 0;
				pt[3].x += 1;	//	矢印の先端に戻る
				pt[3].y += 0;
				pt[4].x += 1;	//	そして右上へ
				pt[4].y += 0;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_NEL:
	case EOL_LS:
	case EOL_PS:
		{
			// 左下矢印(折れ曲がりなし)
			sx = rcEol.left;			//X左端
			sy = rcEol.top + ( rcEol.Height() * 3 / 4 );	//Y上から3/4
			DWORD pp[] = { 2, 3 };
			POINT pt[5];
			int nWidth = t_min(rcEol.Width(), rcEol.Height() / 2);
			pt[0].x = sx + nWidth;	//	右上から
			pt[0].y = sy - nWidth;
			pt[1].x = sx;	//	先頭へ
			pt[1].y = sy;
			pt[2].x = sx + nWidth;	//	右から
			pt[2].y = sy;
			pt[3].x = sx;	//	先頭へ戻り
			pt[3].y = sy;
			pt[4].x = sx;	//	先頭から上へ
			pt[4].y = sy - nWidth;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 0;	//	右上から
				pt[0].y += 1;
				pt[1].x += 0;	//	先頭へ
				pt[1].y += 1;
				pt[2].x += 0;	//	右から
				pt[2].y -= 1;
				pt[3].x += 1;	//	先頭へ戻り
				pt[3].y -= 1;
				pt[4].x += 1;	//	先頭から上へ
				pt[4].y += 0;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	}

	SelectObject(gr, hPenOld);
}
