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

static int32_t& ClumpOffset = *(int32_t*)0x978798;

static AnimBlendFrameData* foundFrame = nullptr;

enum BoneTag {
    BONE_root = 0,
    BONE_pelvis = 1,
    BONE_spine = 2,
    BONE_spine1 = 3,
    BONE_neck = 4,
    BONE_head = 5,
    BONE_l_clavicle = 31,
    BONE_l_upperarm = 32,
    BONE_l_forearm = 33,
    BONE_l_hand = 34,
    BONE_l_finger = 35,
    BONE_r_clavicle = 21,
    BONE_r_upperarm = 22,
    BONE_r_forearm = 23,
    BONE_r_hand = 24,
    BONE_r_finger = 25,
    BONE_l_thigh = 41,
    BONE_l_calf = 42,
    BONE_l_foot = 43,
    BONE_r_thigh = 51,
    BONE_r_calf = 52,
    BONE_r_foot = 53,
};

static const char* ConvertBoneTag2BoneName(int tag) {
    switch (tag) {
        case BONE_root:	return "Root";
        case BONE_pelvis:	return "Pelvis";
        case BONE_spine:	return "Spine";
        case BONE_spine1:	return "Spine1";
        case BONE_neck:	return "Neck";
        case BONE_head:	return "Head";
        case BONE_r_clavicle:	return "Bip01 R Clavicle";
        case BONE_r_upperarm:	return "R UpperArm";
        case BONE_r_forearm:	return "R Forearm";
        case BONE_r_hand:	return "R Hand";
        case BONE_r_finger:	return "R Fingers";
        case BONE_l_clavicle:	return "Bip01 L Clavicle";
        case BONE_l_upperarm:	return "L UpperArm";
        case BONE_l_forearm:	return "L Forearm";
        case BONE_l_hand:	return "L Hand";
        case BONE_l_finger:	return "L Fingers";
        case BONE_l_thigh:	return "L Thigh";
        case BONE_l_calf:	return "L Calf";
        case BONE_l_foot:	return "L Foot";
        case BONE_r_thigh:	return "R Thigh";
        case BONE_r_calf:	return "R Calf";
        case BONE_r_foot:	return "R Foot";
    }
    return nullptr;
}

static int32_t gtastrcmp(const char* str1, const char* str2) {
    return plugin::CallAndReturn<int32_t, 0x642410>(str1, str2);
}

static void FrameFindCallbackSkinned(AnimBlendFrameData* frame, void* arg) {
    const char* name = ConvertBoneTag2BoneName(frame->m_nNodeId);
    if (name && gtastrcmp(name, (char*)arg) == 0)
        foundFrame = frame;
}

static void FrameFindCallback(AnimBlendFrameData* frame, void* arg) {
    const char* name = GetFrameNodeName(frame->m_pFrame);
    if (gtastrcmp(name, (char*)arg) == 0)
        foundFrame = frame;
}

static inline RpAtomic* IsClumpSkinned(RpClump* c) {
    RpAtomic* ret = nullptr;
    RpClumpForAllAtomics(c, [](RpAtomic* atomic, void* data) -> RpAtomic* {
        RpAtomic** ret = (RpAtomic**)data;
        if (*ret)
            return nullptr;

        if (RpSkinGeometryGetSkin(atomic->geometry))
            *ret = atomic;
        return atomic;
    }, &ret);
    return ret;
}

static AnimBlendFrameData* RpAnimBlendClumpFindFrame(RpClump* clump, const char* name) {
    foundFrame = NULL;
    CAnimBlendClumpData* clumpData = *RWPLUGINOFFSET(CAnimBlendClumpData*, clump, ClumpOffset);
    if (IsClumpSkinned(clump))
        clumpData->ForAllFrames(FrameFindCallbackSkinned, (void*)name);
    else
        clumpData->ForAllFrames(FrameFindCallback, (void*)name);
    return foundFrame;
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
        return false;
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

static RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time) {
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
