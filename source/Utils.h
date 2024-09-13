#pragma once
#include "PluginBase.h"
#include "RenderWare.h"

static RpClump* LoadClump(const char* name) {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, name);
    if (!stream)
        return nullptr;

    if (!RwStreamFindChunk(stream, rwID_CLUMP, NULL, NULL))
        return nullptr;

    RpClump* out = RpClumpStreamRead(stream);
    RwStreamClose(stream, NULL);

    return out;
}

static int32_t RwStreamReadInt32(RwStream* stream) {
    int32_t out;
    ::RwStreamReadInt32(stream, &out, sizeof(int32_t));
    return out;
}

static RwReal RwStreamReadReal(RwStream* stream) {
    float out;
    ::RwStreamReadReal(stream, &out, sizeof(float));
    return out;
}

static RpHAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream) {
    RpHAnimAnimation* anim = nullptr;
    if (RwStreamReadInt32(stream) != 0x100)
        return nullptr;
    int32_t typeID = RwStreamReadInt32(stream);
    int32_t numFrames = RwStreamReadInt32(stream);
    int32_t flags = RwStreamReadInt32(stream);
    float duration = RwStreamReadReal(stream);

    anim = RtAnimAnimationCreate(typeID, numFrames, flags, duration);

    RpHAnimKeyFrameStreamRead(stream, anim);

    return anim;
}

static RpHAnimAnimation* RtAnimAnimationRead(const char* name) {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, name);
    if (!stream)
        return nullptr;

    if (!RwStreamFindChunk(stream, rwID_HANIMANIMATION, nullptr, nullptr))
        return nullptr;

    auto out = RtAnimAnimationStreamRead(stream);
    RwStreamClose(stream, nullptr);

    return out;
}

static int32_t& RpHAnimAtomicGlobals = *(int32_t*)0x944088;

static void RpHAnimFrameSetHierarchy(RwFrame* frame, RpHAnimHierarchy* hierarchy) {
    struct HAnimData {
        int32_t id;
        RpHAnimHierarchy* hierarchy;
    };

    ((HAnimData*)((uint8_t*)(frame)+(RpHAnimAtomicGlobals)))->hierarchy = hierarchy;
    hierarchy->parentFrame = frame;
}

static RpAtomic* GetAnimHierarchyCallback(RpAtomic* atomic, void* data) {
    *(RpHAnimHierarchy**)data = RpSkinAtomicGetHAnimHierarchy(atomic);
    return NULL;
}

static RpHAnimHierarchy* GetAnimHierarchyFromSkinClump(RpClump* clump) {
    RpHAnimHierarchy* hier = NULL;
    RpClumpForAllAtomics(clump, GetAnimHierarchyCallback, &hier);
    return hier;
}

struct KeyFrameHeader {
    KeyFrameHeader* prev;
    float time;

    KeyFrameHeader* next(int32_t sz) {
        return (KeyFrameHeader*)((uint8_t*)this + sz);
    }
};

struct InterpFrameHeader {
    KeyFrameHeader* keyFrame1;
    KeyFrameHeader* keyFrame2;
};

static KeyFrameHeader* RtAnimInterpolatorGetAnimFrame(RtAnimInterpolator* anim, int32_t n) {
    return (KeyFrameHeader*)((uint8_t*)anim->pCurrentAnim->pFrames +
                             n * anim->currentKeyFrameSize);
}

static void* RtAnimInterpolatorGetFrames(RtAnimInterpolator* anim) {
    return anim + 1;
}

static InterpFrameHeader* RtAnimInterpolatorGetInterpFrame(RtAnimInterpolator* anim, int32_t n) {
    return (InterpFrameHeader*)((uint8_t*)RtAnimInterpolatorGetFrames(anim) +
                                n * anim->currentKeyFrameSize);
}

static RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* anim, RwReal t) {
    if (t <= 0.0f)
        return true;

    anim->currentTime += t;

    if (anim->currentTime > anim->pCurrentAnim->duration) {
        RtAnimInterpolatorSetCurrentAnim(anim, anim->pCurrentAnim);
        return true;
    }

    KeyFrameHeader* last = RtAnimInterpolatorGetAnimFrame(anim, anim->pCurrentAnim->numFrames);
    KeyFrameHeader* next = (KeyFrameHeader*)anim->pNextFrame;
    InterpFrameHeader* ifrm = nullptr;

    while (next < last && next->prev->time <= anim->currentTime) {
        for (int32_t i = 0; i < anim->numNodes; i++) {
            ifrm = RtAnimInterpolatorGetInterpFrame(anim, i);
            if (ifrm->keyFrame2 == next->prev)
                break;
        }
        ifrm->keyFrame1 = ifrm->keyFrame2;
        ifrm->keyFrame2 = next;

        next = (KeyFrameHeader*)((uint8_t*)anim->pNextFrame + anim->currentKeyFrameSize);
        anim->pNextFrame = next;
    }
    for (int32_t i = 0; i < anim->numNodes; i++) {
        ifrm = RtAnimInterpolatorGetInterpFrame(anim, i);
        anim->keyFrameInterpolateCB(ifrm, ifrm->keyFrame1, ifrm->keyFrame2,
                       anim->currentTime);
    }

    return true;
}

static RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal t) {
    anim->currentTime -= t;

    if (anim->currentTime < 0.0f) {
        anim->currentTime = 0.0f;
        RtAnimInterpolatorSetCurrentAnim(anim, anim->pCurrentAnim);
        return true;
    }

    KeyFrameHeader* first = RtAnimInterpolatorGetAnimFrame(anim, 0);
    KeyFrameHeader* prev = (KeyFrameHeader*)anim->pNextFrame;
    InterpFrameHeader* ifrm = nullptr;

    while (prev > first && prev->time >= anim->currentTime) {
        prev = (KeyFrameHeader*)((uint8_t*)anim->pNextFrame - anim->currentKeyFrameSize);
        anim->pNextFrame = prev;
    }

    for (int32_t i = 0; i < anim->numNodes; i++) {
        ifrm = RtAnimInterpolatorGetInterpFrame(anim, i);
        if (ifrm->keyFrame1 == prev)
            break;
    }
    ifrm->keyFrame2 = ifrm->keyFrame1;
    ifrm->keyFrame1 = prev;

    for (int32_t i = 0; i < anim->numNodes; i++) {
        ifrm = RtAnimInterpolatorGetInterpFrame(anim, i);
        anim->keyFrameInterpolateCB(ifrm, ifrm->keyFrame1, ifrm->keyFrame2, anim->currentTime);
    }

    return true;
}

static RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time) {
    RwReal t = time - anim->currentTime;
    if (time < 0.0f)
        RtAnimInterpolatorSubAnimTime(anim, -t);
    else {
        RtAnimInterpolatorAddAnimTime(anim, t);
    }

    return true;
}
