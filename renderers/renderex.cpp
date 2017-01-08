#include <QOpenGLWidget>
#include <QOpenGLFramebufferObject>
#include <stdexcept>
#include <algorithm>
#include "renderlegacy2d.h"
#include "mesh/mesh.h"
#include "settings/settings.h"
#include "interface/renwin2d.h"
#include "renderex.h"

#include "nanovg/nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#ifdef NANOVG_GL3_IMPLEMENTATION
    #include <QOpenGLFunctions_3_2_Core>
#else
    #include <QOpenGLFunctions_2_0>
#endif
#include "nanovg/nanovg_gl.h"

Renderer2Dex::Renderer2Dex() {}

Renderer2Dex::~Renderer2Dex() {}

void Renderer2Dex::Init()
{
#ifdef NANOVG_GL3_IMPLEMENTATION
    qgl = new QOpenGLFunctions_3_2_Core;
    qgl->initializeOpenGLFunctions();
    vg = nvgCreateGL3(0);
#else
    qgl = new QOpenGLFunctions_2_0;
    qgl->initializeOpenGLFunctions();
    vg = nvgCreateGL2(0);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.7f, 0.7f, 0.7f, 0.7f);
    CreateFoldTextures();
}

void Renderer2Dex::ResizeView(int w, int h)
{
    m_width = w;
    m_height = h;
    glViewport(0, 0, w, h);
}

void Renderer2Dex::RecalcProjection()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float hwidth = m_cameraPosition[2] * float(m_width)/float(m_height);
    glOrtho(-hwidth, hwidth, -m_cameraPosition[2], m_cameraPosition[2], 0.1f, 2000.0f);

    m_scale = m_height/m_cameraPosition[2];  // -> W/(Z * W/H);
}

void Renderer2Dex::SetupNvgView() const
{
    //coordinate system: center & flip vertical
    nvgTranslate(vg, m_width/2, m_height/2);
    nvgScale(vg, 1, -1);

    //camera: scale & translate
    nvgScale(vg, m_scale/2, m_scale/2);
    nvgTranslate(vg, m_cameraPosition[0], m_cameraPosition[1]);
}

void Renderer2Dex::PreDraw() const
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(m_cameraPosition[0], m_cameraPosition[1], -1.0f);
}

void Renderer2Dex::DrawSelection(const SSelectionInfo& sinfo) const
{
    nvgBeginFrame(vg, m_width, m_height, 1);
    SetupNvgView();
    nvgStrokeWidth(vg, 3/m_scale);

    if(sinfo.m_editMode == (int)CRenWin2D::EM_SNAP ||
       sinfo.m_editMode == (int)CRenWin2D::EM_CHANGE_FLAPS ||
       sinfo.m_editMode == (int)CRenWin2D::EM_ROTATE)
    {
        CMesh::STriangle2D* trUnderCursor = nullptr;
        int edgeUnderCursor = 0;
        m_model->GetStuffUnderCursor(sinfo.m_mouseWorldPos, trUnderCursor, edgeUnderCursor);

        if(sinfo.m_editMode == (int)CRenWin2D::EM_ROTATE && sinfo.m_triangle)
        {
            trUnderCursor = (CMesh::STriangle2D*)sinfo.m_triangle;
            edgeUnderCursor = sinfo.m_edge;
        }

        //highlight edge under cursor (if it has neighbour)
        if(trUnderCursor && (trUnderCursor->GetEdge(edgeUnderCursor)->HasTwoTriangles() ||
                             sinfo.m_editMode == (int)CRenWin2D::EM_ROTATE))
        {
            if(sinfo.m_editMode == (int)CRenWin2D::EM_CHANGE_FLAPS &&
               trUnderCursor->GetEdge(edgeUnderCursor)->IsSnapped())
                return;

            const glm::vec2 &v1 = (*trUnderCursor)[0];
            const glm::vec2 &v2 = (*trUnderCursor)[1];
            const glm::vec2 &v3 = (*trUnderCursor)[2];

            NVGcolor color;

            if(sinfo.m_editMode == (int)CRenWin2D::EM_CHANGE_FLAPS)
            {
                color = nvgRGBf(0.0f, 0.0f, 1.0f);
            } else if(sinfo.m_editMode == (int)CRenWin2D::EM_SNAP) {
                if(trUnderCursor->GetEdge(edgeUnderCursor)->IsSnapped())
                    color = nvgRGBf(1.0f, 0.0f, 0.0f);
                else
                    color = nvgRGBf(0.0f, 1.0f, 0.0f);
            } else {
                color = nvgRGBf(0.0f, 1.0f, 1.0f);
            }

            glm::vec2 a, b;
            glm::vec2 e1Middle;
            switch(edgeUnderCursor)
            {
                case 0:
                    a = v1; b = v2;
                    e1Middle = 0.5f*(v1+v2);
                    break;
                case 1:
                    a = v3; b = v2;
                    e1Middle = 0.5f*(v3+v2);
                    break;
                case 2:
                    a = v1; b = v3;
                    e1Middle = 0.5f*(v1+v3);
                    break;
                default : break;
            }
            if (edgeUnderCursor <= 2)
            {
                nvgBeginPath(vg);
                nvgMoveTo(vg, a[0], a[1]);
                nvgLineTo(vg, b[0], b[1]);
                nvgStrokeColor(vg, color);
                nvgStroke(vg);
            }

            if(sinfo.m_editMode != (int)CRenWin2D::EM_ROTATE)
            {
                const CMesh::STriangle2D* tr2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriangle(trUnderCursor);
                int e2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriIndex(trUnderCursor);
                const glm::vec2 v12 = (*tr2)[0];
                const glm::vec2 v22 = (*tr2)[1];
                const glm::vec2 v32 = (*tr2)[2];

                glm::vec2 a, b, c, d;
                switch(e2)
                {
                    case 0:
                        a = v12; b = v22;
                        d = 0.5f*(v12+v22);
                        break;
                    case 1:
                        a = v32; b = v22;
                        d = 0.5f*(v32+v22);
                        break;
                    case 2:
                        a = v12; b = v32;
                        d = 0.5f*(v12+v32);
                        break;
                    default : break;
                }
                c = e1Middle;
                if (e2 <= 2)
                {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, a[0], a[1]);
                    nvgLineTo(vg, b[0], b[1]);
                    nvgMoveTo(vg, c[0], c[1]);
                    nvgLineTo(vg, d[0], d[1]);
                    nvgStroke(vg);
                }
            }
        }
    } else {
        //draw selection rectangle - CRenWin2D::EM_MOVE
        if(sinfo.m_group)
        {
            const CMesh::STriGroup* tGroup = (const CMesh::STriGroup*)sinfo.m_group;
            glm::vec2 pos = tGroup->GetPosition();
            float w = tGroup->GetAABBHalfSide() * 2;
            nvgBeginPath(vg);
            nvgRect(vg, pos[0]-w/2, pos[1]-w/2, w, w);
            nvgStrokeColor(vg, nvgRGBf(1.0f, 0.0f, 0.0f));
            nvgStroke(vg);
        }
    }

    nvgEndFrame(vg);
}

void Renderer2Dex::DrawPaperSheet(const glm::vec2 &position, const glm::vec2 &size) const
{
    nvgBeginFrame(vg, m_width, m_height, 1);
    SetupNvgView();
    const float unit = 1/m_scale;

    //shadow
    const float shadowOffset = 1;
    nvgBeginPath(vg);
    nvgRect(vg, position.x+shadowOffset, position.y-shadowOffset,
                size.x, size.y);
    nvgFillColor(vg, nvgRGBf(0.3,0.3,0.3));
    nvgFill(vg);

    //page
    nvgBeginPath(vg);
    nvgRect(vg, position.x, position.y, size.x, size.y);
    nvgFillColor(vg, nvgRGB(255,255,255));
    nvgStrokeColor(vg, nvgRGB(0,0,0));
    nvgStrokeWidth(vg, 1.5*unit);
    nvgFill(vg);
    nvgStroke(vg);

    nvgEndFrame(vg);
}

void Renderer2Dex::DrawScene() const
{
    if(!m_model)
        return;

    DrawParts();
}

void Renderer2Dex::DrawParts() const
{
    const unsigned char renFlags = CSettings::GetInstance().GetRenderFlags();

    if(renFlags & CSettings::R_FLAPS)
    {
        DrawFlaps();
    }

    DrawGroups();

    if(renFlags & (CSettings::R_EDGES | CSettings::R_FOLDS))
    {
        DrawEdges();
    }
}

void Renderer2Dex::DrawFlaps() const
{
    nvgBeginFrame(vg, m_width, m_height, 1);
    SetupNvgView();
    nvgLineJoin(vg, NVG_BEVEL);

    for(const CMesh::SEdge &e : m_model->GetEdges())
    {
        if(!e.IsSnapped())
        {
            switch(e.GetFlapPosition())
            {
            case CMesh::SEdge::FP_LEFT:
                RenderFlap(e.GetTriangle(0), e.GetTriIndex(0));
                break;
            case CMesh::SEdge::FP_RIGHT:
                RenderFlap(e.GetTriangle(1), e.GetTriIndex(1));
                break;
            case CMesh::SEdge::FP_BOTH:
                RenderFlap(e.GetTriangle(0), e.GetTriIndex(0));
                RenderFlap(e.GetTriangle(1), e.GetTriIndex(1));
                break;
            case CMesh::SEdge::FP_NONE:
            default:
                break;
            }
        }
    }

    nvgEndFrame(vg);
}

void Renderer2Dex::DrawGroups() const
{
    const std::vector<glm::vec2> &uvs = m_model->GetUVCoords();

    glBegin(GL_TRIANGLES);
    const auto &parts = m_model->GetGroups();
    for(auto it=parts.begin(); it!=parts.end(); ++it)
    {
        const CMesh::STriGroup &grp = *it;
        const std::list<CMesh::STriangle2D*>& grpTris = grp.GetTriangles();

        for(auto it2=grpTris.begin(), itEnd = grpTris.end(); it2!=itEnd; ++it2)
        {
            const CMesh::STriangle2D& tr2D = **it2;
            const glm::uvec4 &t = m_model->GetTriangles()[tr2D.ID()];

            BindTexture(t[3]);

            const glm::vec2 vertex1 = tr2D[0];
            const glm::vec2 vertex2 = tr2D[1];
            const glm::vec2 vertex3 = tr2D[2];

            const glm::vec2 &uv1 = uvs[t[0]];
            const glm::vec2 &uv2 = uvs[t[1]];
            const glm::vec2 &uv3 = uvs[t[2]];

            glTexCoord2f(uv1[0], uv1[1]);
            glVertex2f(vertex1[0], vertex1[1]);

            glTexCoord2f(uv2[0], uv2[1]);
            glVertex2f(vertex2[0], vertex2[1]);

            glTexCoord2f(uv3[0], uv3[1]);
            glVertex2f(vertex3[0], vertex3[1]);

        }
    }
    glEnd();

    UnbindTexture();
}

void Renderer2Dex::DrawEdges() const
{
    const unsigned char renFlags = CSettings::GetInstance().GetRenderFlags();

    nvgBeginFrame(vg, m_width, m_height, 1);
    SetupNvgView();

    for(const CMesh::SEdge &e : m_model->GetEdges())
    {
        int foldType = (int)e.GetFoldType();
        if(foldType == CMesh::SEdge::FT_FLAT && e.IsSnapped())
            continue;

     if(e.HasTwoTriangles())
     {
         if(e.IsSnapped() && (renFlags & CSettings::R_FOLDS))
         {
             RenderEdge(e.GetTriangle(0), e.GetTriIndex(0), foldType);
         } else if(!e.IsSnapped() && (renFlags & CSettings::R_EDGES)) {
             RenderEdge(e.GetTriangle(0), e.GetTriIndex(0), CMesh::SEdge::FT_FLAT);
             RenderEdge(e.GetTriangle(1), e.GetTriIndex(1), CMesh::SEdge::FT_FLAT);
         }
     } else if(renFlags & CSettings::R_EDGES) {
         CMesh::STriangle2D *t = e.GetAnyTriangle();
         int edge = e.GetAnyTriIndex();
         RenderEdge(t, edge, CMesh::SEdge::FT_FLAT);
     }
    }

    nvgEndFrame(vg);
}

//flaps are not filled - valid flap is on a sheet, so it'll be white
void Renderer2Dex::RenderFlap(CMesh::STriangle2D *tr, int edge) const
{
    const CMesh::STriangle2D& t = *tr;
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];
    const glm::vec2 vN = t.GetNormal(edge) * 0.5f;

    nvgBeginPath(vg);
    nvgMoveTo(vg, v1.x, v1.y);

    if(t.IsFlapSharp(edge))
    {
        float bx = 0.5f*v1.x + 0.5f*v2.x + vN.x;
        float by = 0.5f*v1.y + 0.5f*v2.y + vN.y;
        nvgLineTo(vg, bx, by);
    } else {
        float bx = 0.9f*v1.x + 0.1f*v2.x + vN.x;
        float by = 0.9f*v1.y + 0.1f*v2.y + vN.y;
        float cx = 0.1f*v1.x + 0.9f*v2.x + vN.x;
        float cy = 0.1f*v1.y + 0.9f*v2.y + vN.y;
        nvgLineTo(vg, bx, by);
        nvgLineTo(vg, cx, cy);
    }
    nvgLineTo(vg, v2.x, v2.y);
    nvgStrokeColor(vg, nvgRGB(0,0,0));
    nvgStrokeWidth(vg, 0.025);
    nvgStroke(vg);
}

void Renderer2Dex::RenderEdge(CMesh::STriangle2D *tr, int edge, int foldType) const
{
    const CMesh::STriangle2D& t = *tr;
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];

    nvgBeginPath(vg);
    nvgMoveTo(vg, v1.x, v1.y);
    nvgLineTo(vg, v2.x, v2.y);
    switch(foldType)
    {
    case CMesh::SEdge::FT_FLAT:
        nvgStrokeColor(vg, nvgRGBA(0,0,0,192));
        nvgStrokeWidth(vg, std::max(0.025f, 2*1/m_scale));
        break;
    case CMesh::SEdge::FT_VALLEY:  //todo dashes?
        nvgStrokeColor(vg, nvgRGBA(0,64,64,192));
        nvgStrokeWidth(vg, 0.025);
        break;
    case CMesh::SEdge::FT_MOUNTAIN:
        nvgStrokeColor(vg, nvgRGBA(128,128,128,128));
        nvgStrokeWidth(vg, 0.025);
        break;
    default: assert(false);
    }
    nvgStroke(vg);
}

QImage Renderer2Dex::DrawImageFromSheet(const glm::vec2 &pos) const
{
    const CSettings& sett = CSettings::GetInstance();

    const int papW = sett.GetPaperWidth();
    const int papH = sett.GetPaperHeight();
    const int fboW = (int)(papW * sett.GetResolutionScale());
    const int fboH = (int)(papH * sett.GetResolutionScale());

    glViewport(0, 0, fboW, fboH);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-papW * 0.05f, papW * 0.05f, -papH * 0.05f, papH * 0.05f, 0.1f, 2000.0f);

    glMatrixMode(GL_MODELVIEW);

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setSamples(6);
    fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
    fboFormat.setTextureTarget(GL_TEXTURE_2D_MULTISAMPLE);
    QOpenGLFramebufferObject fbo(fboW, fboH, fboFormat);
    if(!fbo.isValid())
    {
        throw std::logic_error("Failed to create framebuffer object");
    }
    fbo.bind();

    glLoadIdentity();
    glTranslatef(-pos.x - papW*0.05f, -pos.y - papH*0.05f, -1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawParts();

    QImage img(fbo.toImage());

    assert(fbo.release());

    glViewport(0, 0, m_width, m_height);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glClearColor(0.7f, 0.7f, 0.7f, 0.7f);

    return img;
}

void Renderer2Dex::BindTexture(unsigned id) const
{
    const bool renTexture = CSettings::GetInstance().GetRenderFlags() & CSettings::R_TEXTR;
    if(renTexture && m_boundTextureID != (int)id)
    {
        glEnd();
        if(m_textures[id])
        {
            m_textures[id]->bind();
        } else if(m_boundTextureID >= 0 && m_textures[m_boundTextureID])
        {
            m_textures[m_boundTextureID]->release();
        }
        glBegin(GL_TRIANGLES);
        m_boundTextureID = id;
    }
}

void Renderer2Dex::UnbindTexture() const
{
    const bool renTexture = CSettings::GetInstance().GetRenderFlags() & CSettings::R_TEXTR;
    if(renTexture)
    {
        for(auto it=m_textures.begin(); it!=m_textures.end(); it++)
        {
            if(m_textures[it->first] && m_textures[it->first]->isBound())
            {
                m_textures[it->first]->release();
                break;
            }
        }
        m_boundTextureID = -1;
    }
}

void Renderer2Dex::ClearTextures()
{
    m_boundTextureID = -1;
    IRenderer2D::ClearTextures();
}
