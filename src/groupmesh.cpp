//-----------------------------------------------------------------------------
// Routines to generate our watertight brep shells from the operations
// and entities specified by the user in each group; templated to work either
// on an SShell of ratpoly surfaces or on an SMesh of triangles.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

namespace {
constexpr double CIRCLE_KAPPA = 0.5522847498307936;

static void AddLineBezier(SBezierList *sbl, const Vector &a, const Vector &b) {
    SBezier sb = SBezier::From(a, b);
    sbl->l.Add(&sb);
}

static void AddCircleBeziers(SBezierList *sbl, const Vector &center,
                             const Vector &uDir, const Vector &vDir, double radius) {
    if(radius <= LENGTH_EPS) return;

    Vector u = uDir.WithMagnitude(1);
    Vector v = vDir.WithMagnitude(1);
    double k = CIRCLE_KAPPA * radius;

    Vector p0 = center.Plus(u.ScaledBy(radius));
    Vector p1 = center.Plus(v.ScaledBy(radius));
    Vector p2 = center.Minus(u.ScaledBy(radius));
    Vector p3 = center.Minus(v.ScaledBy(radius));

    SBezier b01 = SBezier::From(p0, p0.Plus(v.ScaledBy(k)), p1.Plus(u.ScaledBy(-k)), p1);
    SBezier b12 = SBezier::From(p1, p1.Plus(u.ScaledBy(-k)), p2.Plus(v.ScaledBy(-k)), p2);
    SBezier b23 = SBezier::From(p2, p2.Plus(v.ScaledBy(-k)), p3.Plus(u.ScaledBy(k)), p3);
    SBezier b30 = SBezier::From(p3, p3.Plus(u.ScaledBy(k)), p0.Plus(v.ScaledBy(k)), p0);
    sbl->l.Add(&b01);
    sbl->l.Add(&b12);
    sbl->l.Add(&b23);
    sbl->l.Add(&b30);
}

static void AddArcBezier(SBezierList *sbl, const Vector &center,
                         const Vector &uDir, const Vector &vDir,
                         double radius, double a0, double a1) {
    if(radius <= LENGTH_EPS) return;

    Vector u = uDir.WithMagnitude(1);
    Vector v = vDir.WithMagnitude(1);
    double da = a1 - a0;
    if(fabs(da) <= LENGTH_EPS) return;

    auto PosAt = [&](double a) {
        return center.Plus(u.ScaledBy(radius * cos(a)))
                     .Plus(v.ScaledBy(radius * sin(a)));
    };
    auto TanAt = [&](double a) {
        return u.ScaledBy(-sin(a)).Plus(v.ScaledBy(cos(a)));
    };

    Vector p0 = PosAt(a0);
    Vector p3 = PosAt(a1);
    double alpha = (4.0 / 3.0) * tan(da / 4.0);
    Vector p1 = p0.Plus(TanAt(a0).ScaledBy(radius * alpha));
    Vector p2 = p3.Minus(TanAt(a1).ScaledBy(radius * alpha));

    SBezier sb = SBezier::From(p0, p1, p2, p3);
    sbl->l.Add(&sb);
}

static bool BuildLoopSetsFromBeziers(SBezierList *sbl, SBezierLoopSetSet *out) {
    bool allClosed = false, allCoplanar = false;
    SEdge notClosedAt = {};
    Vector notCoplanarAt = Vector::From(0, 0, 0);
    SPolygon polyLoops = {};
    SBezierLoopSet openContours = {};
    out->FindOuterFacesFrom(sbl, &polyLoops, NULL, SS.ChordTolMm(),
                            &allClosed, &notClosedAt,
                            &allCoplanar, &notCoplanarAt, &openContours);
    bool ok = allClosed && allCoplanar && out->l.n > 0;
    polyLoops.Clear();
    openContours.Clear();
    return ok;
}
} // namespace

void Group::AssembleLoops(bool *allClosed,
                          bool *allCoplanar,
                          bool *allNonZeroLen)
{
    SBezierList sbl = {};

    int i;
    for(auto &e : SK.entity) {
        if(e.group != h)
            continue;
        if(e.construction)
            continue;
        if(e.forceHidden)
            continue;

        e.GenerateBezierCurves(&sbl);
    }

    SBezier *sb;
    *allNonZeroLen = true;
    for(sb = sbl.l.First(); sb; sb = sbl.l.NextAfter(sb)) {
        for(i = 1; i <= sb->deg; i++) {
            if(!(sb->ctrl[i]).Equals(sb->ctrl[0])) {
                break;
            }
        }
        if(i > sb->deg) {
            // This is a zero-length edge.
            *allNonZeroLen = false;
            polyError.errorPointAt = sb->ctrl[0];
            goto done;
        }
    }

    // Try to assemble all these Beziers into loops. The closed loops go into
    // bezierLoops, with the outer loops grouped with their holes. The
    // leftovers, if any, go in bezierOpens.
    bezierLoops.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                                   SS.ChordTolMm(),
                                   allClosed, &(polyError.notClosedAt),
                                   allCoplanar, &(polyError.errorPointAt),
                                   &bezierOpens);
    done:
    sbl.Clear();
}

void Group::GenerateLoops() {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();

    if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE ||
       type == Type::ROTATE || type == Type::TRANSLATE || type == Type::LINKED)
    {
        bool allClosed = false, allCoplanar = false, allNonZeroLen = false;
        AssembleLoops(&allClosed, &allCoplanar, &allNonZeroLen);
        if(!allNonZeroLen) {
            polyError.how = PolyError::ZERO_LEN_EDGE;
        } else if(!allCoplanar) {
            polyError.how = PolyError::NOT_COPLANAR;
        } else if(!allClosed) {
            polyError.how = PolyError::NOT_CLOSED;
        } else {
            polyError.how = PolyError::GOOD;
            // The self-intersecting check is kind of slow, so don't run it
            // unless requested.
            if(SS.checkClosedContour) {
                if(polyLoops.SelfIntersecting(&(polyError.errorPointAt))) {
                    polyError.how = PolyError::SELF_INTERSECTING;
                }
            }
        }
    }
}

void SShell::RemapFaces(Group *g, int remap) {
    for(SSurface &ss : surface){
        hEntity face = { ss.face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        ss.face = face.v;
    }
}

void SMesh::RemapFaces(Group *g, int remap) {
    STriangle *tr;
    for(tr = l.First(); tr; tr = l.NextAfter(tr)) {
        hEntity face = { tr->meta.face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        tr->meta.face = face.v;
    }
}

template<class T>
void Group::GenerateForStepAndRepeat(T *steps, T *outs, Group::CombineAs forWhat) {

    int n = (int)valA, a0 = 0;
    if(subtype == Subtype::ONE_SIDED && skipFirst) {
        a0++; n++;
    }

    int a;
    // create all the transformed copies
    std::vector <T> transd(n);
    std::vector <T> workA(n);
    workA[0] = {};
    // first generate a shell/mesh with each transformed copy
#pragma omp parallel for
    for(a = a0; a < n; a++) {
        transd[a] = {};
        workA[a] = {};
        int ap = a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1));

        if(type == Type::TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            transd[a].MakeFromTransformationOf(steps,
                trans, Quaternion::IDENTITY, 1.0);
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SK.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd[a].MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q, 1.0);
        }
    }
    for(a = a0; a < n; a++) {
        // We need to rewrite any plane face entities to the transformed ones.
        int remap = (a == (n - 1)) ? REMAP_LAST : a;
        transd[a].RemapFaces(this, remap);
    }

    std::vector<T> *soFar = &transd;
    std::vector<T> *scratch = &workA;
    // do the boolean operations on pairs of equal size
    while(n > 1) {
        for(a = 0; a < n; a+=2) {
            scratch->at(a/2).Clear();
            // combine a pair of shells
            if((a==0) && (a0==1)) { // if the first was skipped just copy the 2nd
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a+1)));
                (soFar->at(a+1)).Clear();
                a0 = 0;
            } else if (a == n-1) { // for an odd number just copy the last one
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a)));
                (soFar->at(a)).Clear();
            } else if(forWhat == CombineAs::ASSEMBLE) {
                scratch->at(a/2).MakeFromAssemblyOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            } else {
                scratch->at(a/2).MakeFromUnionOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            }
        }
        swap(scratch, soFar);
        n = (n+1)/2;
    }
    outs->Clear();
    *outs = soFar->at(0);
}

template<class T>
void Group::GenerateForBoolean(T *prevs, T *thiss, T *outs, Group::CombineAs how) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    switch(how) {
        case CombineAs::UNION:
            outs->MakeFromUnionOf(prevs, thiss);
            break;

        case CombineAs::DIFFERENCE:
            outs->MakeFromDifferenceOf(prevs, thiss);
            break;

        case CombineAs::INTERSECTION:
            outs->MakeFromIntersectionOf(prevs, thiss);
            break;

        case CombineAs::ASSEMBLE:
            outs->MakeFromAssemblyOf(prevs, thiss);
            break;
    }
}

void Group::GenerateShellAndMesh() {
    bool prevBooleanFailed = booleanFailed;
    booleanFailed = false;

    Group *srcg = this;

    thisShell.Clear();
    thisMesh.Clear();
    runningShell.Clear();
    runningMesh.Clear();

    // Don't attempt a lathe or extrusion unless the source section is good:
    // planar and not self-intersecting.
    bool haveSrc = true;
    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE) {
        Group *src = SK.GetGroup(opA);
        if(src->polyError.how != PolyError::GOOD) {
            haveSrc = false;
        }
    }

    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's previous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);

        if(!srcg->suppress) {
            if(!IsForcedToMesh()) {
                GenerateForStepAndRepeat<SShell>(&(srcg->thisShell), &thisShell, srcg->meshCombine);
            } else {
                SMesh prevm = {};
                prevm.MakeFromCopyOf(&srcg->thisMesh);
                srcg->thisShell.TriangulateInto(&prevm);
                GenerateForStepAndRepeat<SMesh> (&prevm, &thisMesh, srcg->meshCombine);
            }
        }
    } else if(type == Type::EXTRUDE && haveSrc) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == Subtype::ONE_SIDED || subtype == Subtype::ONE_SKEWED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            int is = thisShell.surface.n;
            // Extrude this outer contour (plus its inner contours, if present)
            thisShell.MakeFromExtrusionOf(sbls, tbot, ttop, color);

            // And for any plane faces, annotate the model with the entity for
            // that face, so that the user can select them with the mouse.
            Vector onOrig = sbls->point;
            int i;
            // Not using range-for here because we're starting at a different place and using
            // indices for meaning.
            for(i = is; i < thisShell.surface.n; i++) {
                SSurface *ss = &(thisShell.surface[i]);
                hEntity face = Entity::NO_ENTITY;

                Vector p = ss->PointAt(0, 0),
                       n = ss->NormalAt(0, 0).WithMagnitude(1);
                double d = n.Dot(p);

                if(i == is || i == (is + 1)) {
                    // These are the top and bottom of the shell.
                    if(fabs((onOrig.Plus(ttop)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_TOP);
                        ss->face = face.v;
                    }
                    if(fabs((onOrig.Plus(tbot)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
                        ss->face = face.v;
                    }
                    continue;
                }

                // So these are the sides
                if(ss->degm != 1 || ss->degn != 1) continue;

                for(Entity &e : SK.entity) {
                    if(e.group != opA) continue;
                    if(e.type != Entity::Type::LINE_SEGMENT) continue;

                    Vector a = SK.GetEntity(e.point[0])->PointGetNum(),
                           b = SK.GetEntity(e.point[1])->PointGetNum();
                    a = a.Plus(ttop);
                    b = b.Plus(ttop);
                    // Could get taken backwards, so check all cases.
                    if((a.Equals(ss->ctrl[0][0]) && b.Equals(ss->ctrl[1][0])) ||
                       (b.Equals(ss->ctrl[0][0]) && a.Equals(ss->ctrl[1][0])) ||
                       (a.Equals(ss->ctrl[0][1]) && b.Equals(ss->ctrl[1][1])) ||
                       (b.Equals(ss->ctrl[0][1]) && a.Equals(ss->ctrl[1][1])))
                    {
                        face = Remap(e.h, REMAP_LINE_TO_FACE);
                        ss->face = face.v;
                        break;
                    }
                }
            }
        }
    } else if(type == Type::LATHE && haveSrc) {
        Group *src = SK.GetGroup(opA);

        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
        }
    } else if(type == Type::REVOLVE && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            if(fabs(anglef - angles) < 2 * PI) {
                thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                      angles, anglef, dists, distf);
            } else {
                thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
            }
        }
    } else if(type == Type::HELIX && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        distf = SK.GetParam(h.param(7))->val * 2; // dist is applied twice
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
            distf *= 0.5;
            dists = -distf;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                  angles, anglef, dists, distf);
        }
    } else if(type == Type::THREAD) {
        Vector pt = SK.GetEntity(predef.origin)->PointGetNum();
        Vector axis = Vector::From(h.param(3), h.param(4), h.param(5));
        if(axis.Magnitude() < LENGTH_EPS) {
            axis = Vector::From(0, 0, 1);
        }
        axis = axis.WithMagnitude(1);
        bool femaleThread = (meshCombine == CombineAs::DIFFERENCE);
        if(femaleThread) {
            // Female threads should start at the selected sketch plane and cut inward.
            axis = axis.ScaledBy(-1);
        }

        double diameter = std::max(SK.GetParam(h.param(6))->val, 1e-6);
        double length = std::max(SK.GetParam(h.param(7))->val, 0.0);
        double threadHeight = std::max(SK.GetParam(h.param(8))->val, 0.0);
        double threadDepth = std::max(SK.GetParam(h.param(9))->val, 0.0);
        double pitch = std::max(SK.GetParam(h.param(10))->val, 1e-6);
        double tipHeight = std::max(SK.GetParam(h.param(11))->val, 0.0);
        double capTopDiameter = std::max(SK.GetParam(h.param(12))->val, 0.0);
        double capDepth = std::max(SK.GetParam(h.param(13))->val, 0.0);
        double capBottomDiameter = std::max(SK.GetParam(h.param(14))->val, 0.0);
        bool tipEnabled = (tipHeight > LENGTH_EPS);
        bool capEnabled = (capDepth > LENGTH_EPS &&
                           capTopDiameter > LENGTH_EPS &&
                           capBottomDiameter > LENGTH_EPS &&
                           capBottomDiameter < capTopDiameter - LENGTH_EPS);
        double effectiveThreadLength = length;
        if(capEnabled) {
            effectiveThreadLength = std::max(effectiveThreadLength - capDepth, 0.0);
        }
        if(tipEnabled) {
            effectiveThreadLength = std::max(effectiveThreadLength - tipHeight, 0.0);
        }
        Vector threadBase = capEnabled ? pt.Plus(axis.ScaledBy(capDepth)) : pt;
        Vector threadEnd = threadBase.Plus(axis.ScaledBy(effectiveThreadLength));
        double nominalRadius = diameter * 0.5;
        double majorRadius = std::max(nominalRadius + threadHeight, 1e-6);
        double minorRadius = std::max(nominalRadius - threadDepth, 1e-6);
        if(majorRadius <= minorRadius + 1e-6) {
            majorRadius = minorRadius + 1e-6;
        }

        Vector u = axis.Normal(0);
        if(u.Magnitude() < LENGTH_EPS) {
            u = Vector::From(1, 0, 0);
        } else {
            u = u.WithMagnitude(1);
        }
        Vector v = axis.Cross(u);
        if(v.Magnitude() < LENGTH_EPS) {
            v = axis.Normal(1);
        } else {
            v = v.WithMagnitude(1);
        }
        u = v.Cross(axis).WithMagnitude(1);

        auto MergeAuxiliaryShell = [&](SShell *auxShell) {
            if(auxShell->IsEmpty()) return;

            if(thisShell.IsEmpty()) {
                thisShell.MakeFromCopyOf(auxShell);
                return;
            }

            SShell merged = {};
            merged.MakeFromUnionOf(&thisShell, auxShell);
            if(!merged.booleanFailed && !merged.IsEmpty()) {
                thisShell.Clear();
                thisShell.MakeFromCopyOf(&merged);
            } else {
                // Keep thread attachments robust when boolean union fails.
                SShell assembled = {};
                assembled.MakeFromAssemblyOf(&thisShell, auxShell);
                thisShell.Clear();
                thisShell.MakeFromCopyOf(&assembled);
                assembled.Clear();
            }
            merged.Clear();
        };

        if(effectiveThreadLength > LENGTH_EPS) {
            // Slight overlap into the support plane to avoid coplanar booleans.
            double overlap = std::max(0.02, std::min(0.10 * pitch, 0.25 * effectiveThreadLength));
            Vector threadStart = threadBase.Minus(axis.ScaledBy(overlap));
            double sweepLength = effectiveThreadLength + overlap;

            // Build the thread profile from the reference model topology:
            // top arc + right cubic flank + bottom arc + left cubic flank.
            // This avoids the alternating major/minor artifact and keeps
            // the outer crest from curving inward ("C" shape).
            constexpr double outerHalfAngle = PI / 8.0;   // 22.5 deg
            constexpr double innerHalfAngle = PI / 4.0;   // 45.0 deg
            constexpr double flankCtrlXScale  = 1.0590950181724821;
            constexpr double flankCtrlY1Scale = 0.5874839274835978;
            constexpr double flankCtrlY2Scale = -0.22984736752498737;
            auto RadialPoint = [&](double radius, double angle) {
                return threadStart.Plus(u.ScaledBy(radius * cos(angle)))
                                 .Plus(v.ScaledBy(radius * sin(angle)));
            };
            double topR = (PI / 2.0) - outerHalfAngle;
            double topL = (PI / 2.0) + outerHalfAngle;
            double botR = (-PI / 2.0) + innerHalfAngle;
            double botL = (-PI / 2.0) - innerHalfAngle;

            Vector pOuterR = RadialPoint(majorRadius, topR);
            Vector pOuterL = RadialPoint(majorRadius, topL);
            Vector pInnerR = RadialPoint(minorRadius, botR);
            Vector pInnerL = RadialPoint(minorRadius, botL);

            Vector cRight1 = threadStart
                .Plus(u.ScaledBy(majorRadius * flankCtrlXScale))
                .Plus(v.ScaledBy(majorRadius * flankCtrlY1Scale));
            Vector cRight2 = threadStart
                .Plus(u.ScaledBy(majorRadius * flankCtrlXScale))
                .Plus(v.ScaledBy(majorRadius * flankCtrlY2Scale));
            Vector cLeft1 = threadStart
                .Plus(u.ScaledBy(-majorRadius * flankCtrlXScale))
                .Plus(v.ScaledBy(majorRadius * flankCtrlY1Scale));
            Vector cLeft2 = threadStart
                .Plus(u.ScaledBy(-majorRadius * flankCtrlXScale))
                .Plus(v.ScaledBy(majorRadius * flankCtrlY2Scale));

            SBezierList threadBeziers = {};
            SBezier rightFlank = SBezier::From(pOuterR, cRight1, cRight2, pInnerR);
            SBezier leftFlank  = SBezier::From(pInnerL, cLeft2, cLeft1, pOuterL);
            threadBeziers.l.Add(&rightFlank);
            AddArcBezier(&threadBeziers, threadStart, u, v, minorRadius, botR, botL);
            threadBeziers.l.Add(&leftFlank);
            AddArcBezier(&threadBeziers, threadStart, u, v, majorRadius, topL, topR);

            double turns = sweepLength / pitch;
            double anglef = turns * (2.0 * PI);

            SBezierLoopSetSet threadLoopSets = {};
            if(BuildLoopSetsFromBeziers(&threadBeziers, &threadLoopSets)) {
                for(SBezierLoopSet *sbls = threadLoopSets.l.First(); sbls;
                    sbls = threadLoopSets.l.NextAfter(sbls)) {
                    thisShell.MakeFromHelicalRevolutionOf(sbls, threadStart, axis, color, this,
                                                          0.0, anglef, 0.0, sweepLength);
                }
            }
            threadLoopSets.Clear();
            threadBeziers.Clear();
        }

        if(capEnabled) {
            double topRadius = capTopDiameter * 0.5;
            double bottomRadius = capBottomDiameter * 0.5;
            Vector capStart = pt;
            Vector capEnd = threadBase;

            SBezierList capProfile = {};
            Vector p0 = capStart;
            Vector p1 = capStart.Plus(u.ScaledBy(topRadius));
            Vector p2 = capEnd.Plus(u.ScaledBy(bottomRadius));
            Vector p3 = capEnd;
            AddLineBezier(&capProfile, p0, p1);
            AddLineBezier(&capProfile, p1, p2);
            AddLineBezier(&capProfile, p2, p3);
            AddLineBezier(&capProfile, p3, p0);

            SBezierLoopSetSet capLoopSets = {};
            if(BuildLoopSetsFromBeziers(&capProfile, &capLoopSets)) {
                SShell capShell = {};
                for(SBezierLoopSet *sbls = capLoopSets.l.First(); sbls;
                    sbls = capLoopSets.l.NextAfter(sbls)) {
                    capShell.MakeFromRevolutionOf(sbls, capStart, axis, color, this);
                }
                MergeAuxiliaryShell(&capShell);
                capShell.Clear();
            }
            capLoopSets.Clear();
            capProfile.Clear();
        }

        if(tipEnabled) {
            // Tip base uses the thread core diameter (minor), not the outer crest diameter.
            double tipBaseRadius = minorRadius;
            // Keep a clearly visible flat on the tip: default about 2 mm.
            double tipFlatDiameter = 2.0 * SS.MmPerUnit();
            // Prevent impossible geometry for very small thread diameters.
            tipFlatDiameter = std::min(tipFlatDiameter, 0.8 * diameter);
            tipFlatDiameter = std::max(tipFlatDiameter, 0.20 * SS.MmPerUnit());
            double tipTopRadius = 0.5 * tipFlatDiameter;
            tipTopRadius = std::min(tipTopRadius, tipBaseRadius * 0.7);
            double tipRadiusDrop = std::max(tipBaseRadius - tipTopRadius, 0.0);

            // Build tip with guaranteed volumetric overlap into the thread body.
            // For short tips, increase overlap so the cone run is not too steep,
            // which otherwise causes unstable booleans (red faces).
            double tipJoinOverlap = std::max(0.05, std::min(0.25 * pitch,
                                                            0.30 * std::max(threadHeight, 0.1)));
            double minConeRun = 0.9 * tipRadiusDrop;
            double neededOverlap = std::max(minConeRun - tipHeight, 0.0);
            tipJoinOverlap = std::max(tipJoinOverlap, neededOverlap);
            if(effectiveThreadLength > LENGTH_EPS) {
                tipJoinOverlap = std::min(tipJoinOverlap, 0.8 * effectiveThreadLength);
            } else {
                tipJoinOverlap = 0.0;
            }

            Vector tipStart = threadEnd.Minus(axis.ScaledBy(tipJoinOverlap));
            Vector tipTop = threadEnd.Plus(axis.ScaledBy(tipHeight));

            SBezierList tipProfile = {};
            // Pure frustum profile (like cap): two diameters, no extra cylinder.
            Vector q0 = tipTop;
            Vector q1 = tipTop.Plus(u.ScaledBy(tipTopRadius));
            Vector q2 = tipStart.Plus(u.ScaledBy(tipBaseRadius));
            Vector q3 = tipStart;
            AddLineBezier(&tipProfile, q0, q1);
            AddLineBezier(&tipProfile, q1, q2);
            AddLineBezier(&tipProfile, q2, q3);
            AddLineBezier(&tipProfile, q3, q0);

            SBezierLoopSetSet tipLoopSets = {};
            if(BuildLoopSetsFromBeziers(&tipProfile, &tipLoopSets)) {
                SShell tipShell = {};
                for(SBezierLoopSet *sbls = tipLoopSets.l.First(); sbls;
                    sbls = tipLoopSets.l.NextAfter(sbls)) {
                    tipShell.MakeFromRevolutionOf(sbls, tipTop, axis, color, this);
                }
                MergeAuxiliaryShell(&tipShell);
                tipShell.Clear();
            }
            tipLoopSets.Clear();
            tipProfile.Clear();
        }
    } else if(type == Type::LINKED) {
        // The imported shell or mesh are copied over, with the appropriate
        // transformation applied. We also must remap the face entities.
        Vector offset = {
            SK.GetParam(h.param(0))->val,
            SK.GetParam(h.param(1))->val,
            SK.GetParam(h.param(2))->val };
        Quaternion q = {
            SK.GetParam(h.param(3))->val,
            SK.GetParam(h.param(4))->val,
            SK.GetParam(h.param(5))->val,
            SK.GetParam(h.param(6))->val };

        thisMesh.MakeFromTransformationOf(&impMesh, offset, q, scale);
        thisMesh.RemapFaces(this, 0);

        thisShell.MakeFromTransformationOf(&impShell, offset, q, scale);
        thisShell.RemapFaces(this, 0);
    }

    if(srcg->meshCombine != CombineAs::ASSEMBLE) {
        thisShell.MergeCoincidentSurfaces();
    }

    // So now we've got the mesh or shell for this group. Combine it with
    // the previous group's mesh or shell with the requested Boolean, and
    // we're done.

    Group *prevg = srcg->RunningMeshGroup();

    if(!IsForcedToMesh()) {
        SShell *prevs = &(prevg->runningShell);
        GenerateForBoolean<SShell>(prevs, &thisShell, &runningShell,
            srcg->meshCombine);

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            runningShell.MergeCoincidentSurfaces();
        }

        // If the Boolean failed, then we should note that in the text screen
        // for this group.
        booleanFailed = runningShell.booleanFailed;
        if(booleanFailed != prevBooleanFailed) {
            SS.ScheduleShowTW();
        }
    } else {
        SMesh prevm, thism;
        prevm = {};
        thism = {};

        prevm.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&prevm);

        thism.MakeFromCopyOf(&thisMesh);
        thisShell.TriangulateInto(&thism);

        SMesh outm = {};
        GenerateForBoolean<SMesh>(&prevm, &thism, &outm, srcg->meshCombine);

        // Remove degenerate triangles; if we don't, they'll get split in SnapToMesh
        // in every generated group, resulting in polynomial increase in triangle count,
        // and corresponding slowdown.
        outm.RemoveDegenerateTriangles();

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            // And make sure that the output mesh is vertex-to-vertex.
            SKdNode *root = SKdNode::From(&outm);
            root->SnapToMesh(&outm);
            root->MakeMeshInto(&runningMesh);
        } else {
            runningMesh.MakeFromCopyOf(&outm);
        }

        outm.Clear();
        thism.Clear();
        prevm.Clear();
    }

    displayDirty = true;
}

void Group::GenerateDisplayItems() {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        Group *pg = RunningMeshGroup();
        if(pg && thisMesh.IsEmpty() && thisShell.IsEmpty()) {
            // We don't contribute any new solid model in this group, so our
            // display items are identical to the previous group's; which means
            // that we can just display those, and stop ourselves from
            // recalculating for those every time we get a change in this group.
            //
            // Note that this can end up recursing multiple times (if multiple
            // groups that contribute no solid model exist in sequence), but
            // that's okay.
            pg->GenerateDisplayItems();

            displayMesh.Clear();
            displayMesh.MakeFromCopyOf(&(pg->displayMesh));

            displayOutlines.Clear();
            if(SS.GW.showEdges || SS.GW.showOutlines) {
                displayOutlines.MakeFromCopyOf(&pg->displayOutlines);
            }
        } else {
            // We do contribute new solid model, so we have to triangulate the
            // shell, and edge-find the mesh.
            displayMesh.Clear();
            runningShell.TriangulateInto(&displayMesh);
            STriangle *t;
            for(t = runningMesh.l.First(); t; t = runningMesh.l.NextAfter(t)) {
                STriangle trn = *t;
                Vector n = trn.Normal();
                trn.an = n;
                trn.bn = n;
                trn.cn = n;
                displayMesh.AddTriangle(&trn);
            }

            displayOutlines.Clear();

            if(SS.GW.showEdges || SS.GW.showOutlines) {
                SOutlineList rawOutlines = {};
                if(!runningMesh.l.IsEmpty()) {
                    // Triangle mesh only; no shell or emphasized edges.
                    runningMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::EMPHASIZED);
                } else {
                    displayMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::SHARP);
                }

                PolylineBuilder builder;
                builder.MakeFromOutlines(rawOutlines);
                builder.GenerateOutlines(&displayOutlines);
                rawOutlines.Clear();
            }
        }

        // If we render this mesh, we need to know whether it's transparent,
        // and we'll want all transparent triangles last, to make the depth test
        // work correctly.
        displayMesh.PrecomputeTransparency();

        // Recalculate mass center if needed
        if(SS.centerOfMass.draw && SS.centerOfMass.dirty && h == SS.GW.activeGroup) {
            SS.UpdateCenterOfMass();
        }
        displayDirty = false;
    }
}

Group *Group::PreviousGroup() const {
    Group *prev = nullptr;
    for(auto const &gh : SK.groupOrder) {
        Group *g = SK.GetGroup(gh);
        if(g->h == h) {
            return prev;
        }
        prev = g;
    }
    return nullptr;
}

Group *Group::RunningMeshGroup() const {
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        return PreviousGroup();
    }
}

bool Group::IsMeshGroup() {
    switch(type) {
        case Group::Type::EXTRUDE:
        case Group::Type::LATHE:
        case Group::Type::REVOLVE:
        case Group::Type::HELIX:
        case Group::Type::THREAD:
        case Group::Type::ROTATE:
        case Group::Type::TRANSLATE:
            return true;

        default:
            return false;
    }
}

void Group::DrawMesh(DrawMeshAs how, Canvas *canvas) {
    if(!(SS.GW.showShaded ||
         SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::VISIBLE)) return;

    switch(how) {
        case DrawMeshAs::DEFAULT: {
            // Force the shade color to something dim to not distract from
            // the sketch.
            Canvas::Fill fillFront = {};
            if(!SS.GW.showShaded) {
                fillFront.layer = Canvas::Layer::DEPTH_ONLY;
            }
            if((type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE)
               && SS.GW.dimSolidModel) {
                fillFront.color = Style::Color(Style::DIM_SOLID);
            }
            Canvas::hFill hcfFront = canvas->GetFill(fillFront);

            // The back faces are drawn in red; should never seem them, since we
            // draw closed shells, so that's a debugging aid.
            Canvas::hFill hcfBack = {};
            if(SS.drawBackFaces && !displayMesh.isTransparent) {
                Canvas::Fill fillBack = {};
                fillBack.layer = fillFront.layer;
                fillBack.color = RgbaColor::FromFloat(1.0f, 0.1f, 0.1f);
                hcfBack = canvas->GetFill(fillBack);
            } else {
                hcfBack = hcfFront;
            }

            // Draw the shaded solid into the depth buffer for hidden line removal,
            // and if we're actually going to display it, to the color buffer too.
            canvas->DrawMesh(displayMesh, hcfFront, hcfBack);

            // Draw mesh edges, for debugging.
            if(SS.GW.showMesh) {
                Canvas::Stroke strokeTriangle = {};
                strokeTriangle.zIndex = 1;
                strokeTriangle.color  = RgbaColor::FromFloat(0.0f, 1.0f, 0.0f);
                strokeTriangle.width  = 1;
                strokeTriangle.unit   = Canvas::Unit::PX;
                Canvas::hStroke hcsTriangle = canvas->GetStroke(strokeTriangle);
                SEdgeList edges = {};
                for(const STriangle &t : displayMesh.l) {
                    edges.AddEdge(t.a, t.b);
                    edges.AddEdge(t.b, t.c);
                    edges.AddEdge(t.c, t.a);
                }
                canvas->DrawEdges(edges, hcsTriangle);
                edges.Clear();
            }
            break;
        }

        case DrawMeshAs::HOVERED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::HOVERED);
            fill.pattern = Canvas::FillPattern::CHECKERED_A;
            fill.zIndex  = 2;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            hEntity he = SS.GW.hover.entity;
            Entity *hovered = (he.v != 0) ? SK.entity.FindByIdNoOops(he) : nullptr;
            if(hovered != nullptr && hovered->IsFace()) {
                faces.push_back(he.v);
            }
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }

        case DrawMeshAs::SELECTED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::SELECTED);
            fill.pattern = Canvas::FillPattern::CHECKERED_B;
            fill.zIndex  = 1;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            SS.GW.GroupSelection();
            auto const &gs = SS.GW.gs;
            // See also GraphicsWindow::MakeSelected "if(c >= MAX_SELECTABLE_FACES)"
            // and GraphicsWindow::GroupSelection "if(e->IsFace())"
            for(auto &fc : gs.face) {
                faces.push_back(fc.v);
            }
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }
    }
}

void Group::Draw(Canvas *canvas) {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    GenerateDisplayItems();
    DrawMesh(DrawMeshAs::DEFAULT, canvas);

    if(SS.GW.showEdges) {
        Canvas::Stroke strokeEdge = Style::Stroke(Style::SOLID_EDGE);
        strokeEdge.zIndex = 1;
        Canvas::hStroke hcsEdge = canvas->GetStroke(strokeEdge);

        canvas->DrawOutlines(displayOutlines, hcsEdge,
                             SS.GW.showOutlines
                             ? Canvas::DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR
                             : Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);

        if(SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::INVISIBLE) {
            Canvas::Stroke strokeHidden = Style::Stroke(Style::HIDDEN_EDGE);
            if(SS.GW.drawOccludedAs == GraphicsWindow::DrawOccludedAs::VISIBLE) {
                strokeHidden.stipplePattern = StipplePattern::CONTINUOUS;
            }
            strokeHidden.layer  = Canvas::Layer::OCCLUDED;
            Canvas::hStroke hcsHidden = canvas->GetStroke(strokeHidden);

            canvas->DrawOutlines(displayOutlines, hcsHidden,
                                 Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);
        }
    }

    if(SS.GW.showOutlines) {
        Canvas::Stroke strokeOutline = Style::Stroke(Style::OUTLINE);
        strokeOutline.zIndex = 1;
        Canvas::hStroke hcsOutline = canvas->GetStroke(strokeOutline);

        canvas->DrawOutlines(displayOutlines, hcsOutline,
                             Canvas::DrawOutlinesAs::CONTOUR_ONLY);
    }
}

void Group::DrawPolyError(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();

    Canvas::Stroke strokeUnclosed = Style::Stroke(Style::DRAW_ERROR);
    strokeUnclosed.color = strokeUnclosed.color.WithAlpha(50);
    Canvas::hStroke hcsUnclosed = canvas->GetStroke(strokeUnclosed);

    Canvas::Stroke strokeError = Style::Stroke(Style::DRAW_ERROR);
    strokeError.layer = Canvas::Layer::FRONT;
    strokeError.width = 1.0f;
    Canvas::hStroke hcsError = canvas->GetStroke(strokeError);

    double textHeight = Style::DefaultTextHeight() / camera.scale;

    // And finally show the polygons too, and any errors if it's not possible
    // to assemble the lines into closed polygons.
    if(polyError.how == PolyError::NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == Type::DRAWING_WORKPLANE) {
            canvas->DrawVectorText(_("not closed contour, or not all same style!"),
                                   textHeight,
                                   polyError.notClosedAt.b, camera.projRight, camera.projUp,
                                   hcsError);
            canvas->DrawLine(polyError.notClosedAt.a, polyError.notClosedAt.b, hcsUnclosed);
        }
    } else if(polyError.how == PolyError::NOT_COPLANAR ||
              polyError.how == PolyError::SELF_INTERSECTING ||
              polyError.how == PolyError::ZERO_LEN_EDGE) {
        // These errors occur at points, not lines
        if(type == Type::DRAWING_WORKPLANE) {
            const char *msg;
            if(polyError.how == PolyError::NOT_COPLANAR) {
                msg = _("points not all coplanar!");
            } else if(polyError.how == PolyError::SELF_INTERSECTING) {
                msg = _("contour is self-intersecting!");
            } else {
                msg = _("zero-length edge!");
            }
            canvas->DrawVectorText(msg, textHeight,
                                   polyError.errorPointAt, camera.projRight, camera.projUp,
                                   hcsError);
        }
    } else {
        // The contours will get filled in DrawFilledPaths.
    }
}

void Group::DrawFilledPaths(Canvas *canvas) {
    for(const SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        // In an assembled loop, all the styles should be the same; so doesn't
        // matter which one we grab.
        const SBezier *sb = &(sbls.l[0].l[0]);
        Style *s = Style::Get({ (uint32_t)sb->auxA });

        Canvas::Fill fill = {};
        fill.zIndex = 1;
        if(s->filled) {
            // This is a filled loop, where the user specified a fill color.
            fill.color = s->fillColor;
        } else if(h == SS.GW.activeGroup && SS.checkClosedContour &&
                    polyError.how == PolyError::GOOD) {
            // If this is the active group, and we are supposed to check
            // for closed contours, and we do indeed have a closed and
            // non-intersecting contour, then fill it dimly.
            fill.color = Style::Color(Style::CONTOUR_FILL).WithAlpha(127);
        } else continue;
        Canvas::hFill hcf = canvas->GetFill(fill);

        SPolygon sp = {};
        sbls.MakePwlInto(&sp);
        canvas->DrawPolygon(sp, hcf);
        sp.Clear();
    }
}

void Group::DrawContourAreaLabels(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();
    Vector gr = camera.projRight.ScaledBy(1 / camera.scale);
    Vector gu = camera.projUp.ScaledBy(1 / camera.scale);

    for(SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        Vector min = sbls.l[0].l[0].ctrl[0];
        Vector max = min;
        Vector zero = Vector::From(0.0, 0.0, 0.0);
        sbls.GetBoundingProjd(Vector::From(1.0, 0.0, 0.0), zero, &min.x, &max.x);
        sbls.GetBoundingProjd(Vector::From(0.0, 1.0, 0.0), zero, &min.y, &max.y);
        sbls.GetBoundingProjd(Vector::From(0.0, 0.0, 1.0), zero, &min.z, &max.z);

        Vector mid = min.Plus(max).ScaledBy(0.5);

        hStyle hs = { Style::CONSTRAINT };
        Canvas::Stroke stroke = Style::Stroke(hs);
        stroke.layer = Canvas::Layer::FRONT;

        std::string label = SS.MmToStringSI(fabs(sbls.SignedArea()), /*dim=*/2);
        double fontHeight = Style::TextHeight(hs);
        double textWidth  = VectorFont::Builtin()->GetWidth(fontHeight, label),
               textHeight = VectorFont::Builtin()->GetCapHeight(fontHeight);
        Vector pos = mid.Minus(gr.ScaledBy(textWidth / 2.0))
                        .Minus(gu.ScaledBy(textHeight / 2.0));
        canvas->DrawVectorText(label, fontHeight, pos, gr, gu, canvas->GetStroke(stroke));
    }
}

} // namespace SolveSpace
