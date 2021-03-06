/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPathRenderer_DEFINED
#define GrPathRenderer_DEFINED

#include "GrDrawContext.h"
#include "GrPaint.h"
#include "GrResourceProvider.h"
#include "GrStyle.h"

#include "SkDrawProcs.h"
#include "SkTArray.h"

class SkPath;
class GrFixedClip;
struct GrPoint;

/**
 *  Base class for drawing paths into a GrDrawTarget.
 *
 *  Derived classes can use stages GrPaint::kTotalStages through GrPipelineBuilder::kNumStages-1.
 *  The stages before GrPaint::kTotalStages are reserved for setting up the draw (i.e., textures and
 *  filter masks).
 */
class SK_API GrPathRenderer : public SkRefCnt {
public:
    GrPathRenderer();

    /**
     * A caller may wish to use a path renderer to draw a path into the stencil buffer. However,
     * the path renderer itself may require use of the stencil buffer. Also a path renderer may
     * use a GrProcessor coverage stage that sets coverage to zero to eliminate pixels that are
     * covered by bounding geometry but outside the path. These exterior pixels would still be
     * rendered into the stencil.
     *
     * A GrPathRenderer can provide three levels of support for stenciling paths:
     * 1) kNoRestriction: This is the most general. The caller sets up the GrPipelineBuilder on the target
     *                    and calls drawPath(). The path is rendered exactly as the draw state
     *                    indicates including support for simultaneous color and stenciling with
     *                    arbitrary stenciling rules. Pixels partially covered by AA paths are
     *                    affected by the stencil settings.
     * 2) kStencilOnly: The path renderer cannot apply arbitrary stencil rules nor shade and stencil
     *                  simultaneously. The path renderer does support the stencilPath() function
     *                  which performs no color writes and writes a non-zero stencil value to pixels
     *                  covered by the path.
     * 3) kNoSupport: This path renderer cannot be used to stencil the path.
     */
    enum StencilSupport {
        kNoSupport_StencilSupport,
        kStencilOnly_StencilSupport,
        kNoRestriction_StencilSupport,
    };

    /**
     * This function is to get the stencil support for a particular path. The path's fill must
     * not be an inverse type. The path will always be filled and not stroked.
     *
     * @param path      the path that will be drawn
     */
    StencilSupport getStencilSupport(const SkPath& path) const {
        SkASSERT(!path.isInverseFillType());
        return this->onGetStencilSupport(path);
    }

    /** Args to canDrawPath()
     *
     * fShaderCaps       The shader caps
     * fPipelineBuilder  The pipelineBuilder
     * fViewMatrix       The viewMatrix
     * fPath             The path to draw
     * fStyle            The styling info (path effect, stroking info)
     * fAntiAlias        True if anti-aliasing is required.
     */
    struct CanDrawPathArgs {
        const GrShaderCaps*         fShaderCaps;
        const SkMatrix*             fViewMatrix;
        const SkPath*               fPath;
        const GrStyle*              fStyle;
        bool                        fAntiAlias;

        // These next two are only used by GrStencilAndCoverPathRenderer
        bool                        fHasUserStencilSettings;
        bool                        fIsStencilBufferMSAA;

        void validate() const {
            SkASSERT(fShaderCaps);
            SkASSERT(fViewMatrix);
            SkASSERT(fPath);
            SkASSERT(fStyle);
            SkASSERT(!fPath->isEmpty());
        }
    };

    /**
     * Returns true if this path renderer is able to render the path. Returning false allows the
     * caller to fallback to another path renderer This function is called when searching for a path
     * renderer capable of rendering a path.
     *
     * @return  true if the path can be drawn by this object, false otherwise.
     */
    bool canDrawPath(const CanDrawPathArgs& args) const {
        SkDEBUGCODE(args.validate();)
        return this->onCanDrawPath(args);
    }

    /**
     * Args to drawPath()
     *
     * fTarget                The target that the path will be rendered to
     * fResourceProvider      The resource provider for creating gpu resources to render the path
     * fPipelineBuilder       The pipelineBuilder
     * fClip                  The clip
     * fColor                 Color to render with
     * fViewMatrix            The viewMatrix
     * fPath                  the path to draw.
     * fStyle                 the style information (path effect, stroke info)
     * fAntiAlias             true if anti-aliasing is required.
     * fGammaCorrect          true if gamma-correct rendering is to be used.
     */
    struct DrawPathArgs {
        GrResourceProvider*         fResourceProvider;
        const GrPaint*              fPaint;
        const GrUserStencilSettings*fUserStencilSettings;

        GrDrawContext*              fDrawContext;
        const GrClip*               fClip;
        GrColor                     fColor;
        const SkMatrix*             fViewMatrix;
        const SkPath*               fPath;
        const GrStyle*              fStyle;
        bool                        fAntiAlias;
        bool                        fGammaCorrect;

        void validate() const {
            SkASSERT(fResourceProvider);
            SkASSERT(fPaint);
            SkASSERT(fUserStencilSettings);
            SkASSERT(fDrawContext);
            SkASSERT(fClip);
            SkASSERT(fViewMatrix);
            SkASSERT(fPath);
            SkASSERT(fStyle);
            SkASSERT(!fPath->isEmpty());
        }
    };

    /**
     * Draws the path into the draw target. If getStencilSupport() would return kNoRestriction then
     * the subclass must respect the stencil settings of the GrPipelineBuilder.
     */
    bool drawPath(const DrawPathArgs& args) {
        SkDEBUGCODE(args.validate();)
#ifdef SK_DEBUG
        CanDrawPathArgs canArgs;
        canArgs.fShaderCaps = args.fResourceProvider->caps()->shaderCaps();
        canArgs.fViewMatrix = args.fViewMatrix;
        canArgs.fPath = args.fPath;
        canArgs.fStyle = args.fStyle;
        canArgs.fAntiAlias = args.fAntiAlias;

        canArgs.fHasUserStencilSettings = !args.fUserStencilSettings->isUnused();
        canArgs.fIsStencilBufferMSAA = args.fDrawContext->isStencilBufferMultisampled();
        SkASSERT(this->canDrawPath(canArgs));
        if (!args.fUserStencilSettings->isUnused()) {
            SkASSERT(kNoRestriction_StencilSupport == this->getStencilSupport(*args.fPath));
            SkASSERT(args.fStyle->isSimpleFill());
        }
#endif
        return this->onDrawPath(args);
    }

    /* Args to stencilPath().
     *
     * fResourceProvider      The resource provider for creating gpu resources to render the path
     * fDrawContext           The target of the draws
     * fViewMatrix            Matrix applied to the path.
     * fPath                  The path to draw.
     * fIsAA                  Is the path to be drawn AA (only set when MSAA is available)
     */
    struct StencilPathArgs {
        GrResourceProvider* fResourceProvider;
        GrDrawContext*      fDrawContext;
        const GrFixedClip*  fClip;
        const SkMatrix*     fViewMatrix;
        const SkPath*       fPath;
        bool                fIsAA;

        void validate() const {
            SkASSERT(fResourceProvider);
            SkASSERT(fDrawContext);
            SkASSERT(fViewMatrix);
            SkASSERT(fPath);
            SkASSERT(!fPath->isEmpty());
        }
    };

    /**
     * Draws the path to the stencil buffer. Assume the writable stencil bits are already
     * initialized to zero. The pixels inside the path will have non-zero stencil values afterwards.
     */
    void stencilPath(const StencilPathArgs& args) {
        SkDEBUGCODE(args.validate();)
        SkASSERT(kNoSupport_StencilSupport != this->getStencilSupport(*args.fPath));
        this->onStencilPath(args);
    }

    // Helper for determining if we can treat a thin stroke as a hairline w/ coverage.
    // If we can, we draw lots faster (raster device does this same test).
    static bool IsStrokeHairlineOrEquivalent(const GrStyle& style, const SkMatrix& matrix,
                                             SkScalar* outCoverage) {
        if (style.pathEffect()) {
            return false;
        }
        const SkStrokeRec& stroke = style.strokeRec();
        if (stroke.isHairlineStyle()) {
            if (outCoverage) {
                *outCoverage = SK_Scalar1;
            }
            return true;
        }
        return stroke.getStyle() == SkStrokeRec::kStroke_Style &&
            SkDrawTreatAAStrokeAsHairline(stroke.getWidth(), matrix, outCoverage);
    }

protected:
    // Helper for getting the device bounds of a path. Inverse filled paths will have bounds set
    // by devSize. Non-inverse path bounds will not necessarily be clipped to devSize.
    static void GetPathDevBounds(const SkPath& path,
                                 int devW,
                                 int devH,
                                 const SkMatrix& matrix,
                                 SkRect* bounds);

private:
    /**
     * Subclass overrides if it has any limitations of stenciling support.
     */
    virtual StencilSupport onGetStencilSupport(const SkPath&) const {
        return kNoRestriction_StencilSupport;
    }

    /**
     * Subclass implementation of drawPath()
     */
    virtual bool onDrawPath(const DrawPathArgs& args) = 0;

    /**
     * Subclass implementation of canDrawPath()
     */
    virtual bool onCanDrawPath(const CanDrawPathArgs& args) const = 0;

    /**
     * Subclass implementation of stencilPath(). Subclass must override iff it ever returns
     * kStencilOnly in onGetStencilSupport().
     */
    virtual void onStencilPath(const StencilPathArgs& args) {
        static constexpr GrUserStencilSettings kIncrementStencil(
            GrUserStencilSettings::StaticInit<
                 0xffff,
                 GrUserStencilTest::kAlways,
                 0xffff,
                 GrUserStencilOp::kReplace,
                 GrUserStencilOp::kReplace,
                 0xffff>()
        );

        GrPaint paint;

        DrawPathArgs drawArgs;
        drawArgs.fResourceProvider = args.fResourceProvider;
        drawArgs.fPaint = &paint;
        drawArgs.fUserStencilSettings = &kIncrementStencil;
        drawArgs.fDrawContext = args.fDrawContext;
        drawArgs.fColor = GrColor_WHITE;
        drawArgs.fViewMatrix = args.fViewMatrix;
        drawArgs.fPath = args.fPath;
        drawArgs.fStyle = &GrStyle::SimpleFill();
        drawArgs.fAntiAlias = false;  // In this case the MSAA handles the AA so we want to draw BW
        drawArgs.fGammaCorrect = false;
        this->drawPath(drawArgs);
    }

    typedef SkRefCnt INHERITED;
};

#endif
