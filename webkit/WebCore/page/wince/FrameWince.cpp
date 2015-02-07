

#include "config.h"
#include "Frame.h"

#include "Document.h"
#include "EditorClient.h"
#include "FloatRect.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "KeyboardEvent.h"
#include "NP_jsobject.h"
#include "npruntime_impl.h"
#include "Page.h"
#include "Plugin.h"
#include "RegularExpression.h"
#include "RenderFrame.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "runtime_root.h"
#include "Settings.h"
#include "TextResourceDecoder.h"

#include <windows.h>

using std::min;

namespace WebCore {

using namespace HTMLNames;

extern HDC g_screenDC;

void computePageRectsForFrame(Frame* frame, const IntRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, Vector<IntRect>& pages, int& outPageHeight)
{
    ASSERT(frame);

    pages.clear();
    outPageHeight = 0;

    if (!frame->document() || !frame->view() || !frame->document()->renderer())
        return;

    RenderView* root = toRenderView(frame->document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }

    float ratio = (float)printRect.height() / (float)printRect.width();

    float pageWidth  = (float) root->overflowWidth();
    float pageHeight = pageWidth * ratio;
    outPageHeight = (int) pageHeight;   // this is the height of the page adjusted by margins
    pageHeight -= (headerHeight + footerHeight);

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight      = root->layer()->height();
    float docWidth       = root->layer()->width();
    float currPageWidth  = pageWidth / userScaleFactor;


    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0.0;
    do {
        float proposedBottom = min(docHeight, printedPagesHeight + pageHeight);
        frame->view()->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);

        pages.append(IntRect(0, printedPagesHeight, currPageWidth, currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

HBITMAP imageFromSelection(Frame* frame, bool forceBlackText)
{
    if (!frame->view())
        return 0;

    frame->view()->setPaintRestriction(forceBlackText ? PaintRestrictionSelectionOnlyBlackText : PaintRestrictionSelectionOnly);
    FloatRect fr = frame->selectionBounds();
    IntRect ir((int)fr.x(), (int)fr.y(), (int)fr.width(), (int)fr.height());
    if (ir.isEmpty())
        return 0;

    int w;
    int h;
    FrameView* view = frame->view();
    if (view->parent()) {
        ir.setLocation(view->parent()->convertChildToSelf(view, ir.location()));
        w = ir.width() * view->scale() + 0.5;
        h = ir.height() * view->scale() + 0.5;
    } else {
        ir = view->contentsToWindow(ir);
        w = ir.width();
        h = ir.height();
    }

    OwnPtr<HDC> bmpDC(CreateCompatibleDC(g_screenDC));
    HBITMAP hBmp = MemoryManager::createCompatibleBitmap(g_screenDC, w, h);
    if (!hBmp)
        return 0;

    HBITMAP hbmpOld = (HBITMAP)SelectObject(bmpDC.get(), hBmp);

    {
        GraphicsContext gc(bmpDC.get());
        frame->document()->updateLayout();
        view->paint(&gc, ir);
    }

    SelectObject(bmpDC.get(), hbmpOld);

    frame->view()->setPaintRestriction(PaintRestrictionNone);

    return hBmp;
}

DragImageRef Frame::dragImageForSelection()
{
    if (selection()->isRange())
        return imageFromSelection(this, false);

    return 0;
}

} // namespace WebCore