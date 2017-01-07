#include <QOpenGLWidget>
#include <QOpenGLFramebufferObject>
#include <stdexcept>
#include "renderlegacy2d.h"
#include "mesh/mesh.h"
#include "settings/settings.h"
#include "renwin2d.h"
#include "renderex.h"

#include "nanovg/nanovg.h"
#include <QOpenGLFunctions_2_0>
#define NANOVG_GL2_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"

Renderer2Dex::Renderer2Dex() {}

Renderer2Dex::~Renderer2Dex() {}

void Renderer2Dex::Init()
{
    QOpenGLFunctions_2_0 *gl = new QOpenGLFunctions_2_0;
    gl->initializeOpenGLFunctions();
    qgl = gl;
    vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_MULTISAMPLE);
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
    glClear(GL_DEPTH_BUFFER_BIT);

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

            glm::vec2 e1Middle;

            if(sinfo.m_editMode == (int)CRenWin2D::EM_CHANGE_FLAPS)
            {
                glColor3f(0.0f, 0.0f, 1.0f);
            } else if(sinfo.m_editMode == (int)CRenWin2D::EM_SNAP) {
                if(trUnderCursor->GetEdge(edgeUnderCursor)->IsSnapped())
                    glColor3f(1.0f, 0.0f, 0.0f);
                else
                    glColor3f(0.0f, 1.0f, 0.0f);
            } else {
                glColor3f(0.0f, 1.0f, 1.0f);
            }
            glLineWidth(3.0f);
            glBegin(GL_LINES);
            switch(edgeUnderCursor)
            {
                case 0:
                    glVertex2f(v1[0], v1[1]);
                    glVertex2f(v2[0], v2[1]);
                    e1Middle = 0.5f*(v1+v2);
                    break;
                case 1:
                    glVertex2f(v3[0], v3[1]);
                    glVertex2f(v2[0], v2[1]);
                    e1Middle = 0.5f*(v3+v2);
                    break;
                case 2:
                    glVertex2f(v1[0], v1[1]);
                    glVertex2f(v3[0], v3[1]);
                    e1Middle = 0.5f*(v1+v3);
                    break;
                default : break;
            }

            if(sinfo.m_editMode != (int)CRenWin2D::EM_ROTATE)
            {
                const CMesh::STriangle2D* tr2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriangle(trUnderCursor);
                int e2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriIndex(trUnderCursor);
                const glm::vec2 &v12 = (*tr2)[0];
                const glm::vec2 &v22 = (*tr2)[1];
                const glm::vec2 &v32 = (*tr2)[2];

                switch(e2)
                {
                    case 0:
                        glVertex2f(v12[0], v12[1]);
                        glVertex2f(v22[0], v22[1]);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v12+v22);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    case 1:
                        glVertex2f(v32[0], v32[1]);
                        glVertex2f(v22[0], v22[1]);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v32+v22);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    case 2:
                        glVertex2f(v12[0], v12[1]);
                        glVertex2f(v32[0], v32[1]);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v12+v32);
                        glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    default : break;
                }
            }
            glEnd();
            glLineWidth(1.0f);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
    } else {
        //draw selection rectangle
        if(sinfo.m_group)
        {
            const CMesh::STriGroup* tGroup = (const CMesh::STriGroup*)sinfo.m_group;
            glColor3f(1.0f, 0.0f, 0.0f);
            glBegin(GL_LINE_LOOP);
            glm::vec2 pos = tGroup->GetPosition();
            float aabbxh = tGroup->GetAABBHalfSide();
            glVertex2f(pos[0]-aabbxh, pos[1]+aabbxh);
            glVertex2f(pos[0]+aabbxh, pos[1]+aabbxh);
            glVertex2f(pos[0]+aabbxh, pos[1]-aabbxh);
            glVertex2f(pos[0]-aabbxh, pos[1]-aabbxh);
            glEnd();
            glColor3f(1.0f, 1.0f, 1.0f);
        }
    }
}

void Renderer2Dex::DrawPaperSheet(const glm::vec2 &position, const glm::vec2 &size) const
{
    nvgBeginFrame(vg, m_width, m_height, 1);

    //coordinate system: center & flip vertical
    nvgTranslate(vg, m_width/2, m_height/2);
    nvgScale(vg, 1, -1);

    //camera: scale & translate
    float scale = m_height/m_cameraPosition[2];  // -> W/(Z * W/H);
    nvgScale(vg, scale/2, scale/2);
    nvgTranslate(vg, m_cameraPosition[0], m_cameraPosition[1]);
    float unit = 1/scale;

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

    glClear(GL_DEPTH_BUFFER_BIT);

    DrawParts();
}

void Renderer2Dex::DrawParts() const
{
    const unsigned char renFlags = CSettings::GetInstance().GetRenderFlags();

    if(renFlags & CSettings::R_FLAPS)
    {
        DrawFlaps();
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    DrawGroups();

    if(renFlags & (CSettings::R_EDGES | CSettings::R_FOLDS))
    {
        DrawEdges();
    }
}

void Renderer2Dex::DrawFlaps() const
{
    if(m_texFolds)
        m_texFolds->bind();

    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 1.0f); //base color
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
    glEnd();

    if(m_texFolds && m_texFolds->isBound())
        m_texFolds->release();
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

    //for dotted line style
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    if(m_texFolds)
        m_texFolds->bind();

    glBegin(GL_QUADS);
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
         void *t = static_cast<void*>(e.GetAnyTriangle());
         int edge = e.GetAnyTriIndex();
         RenderEdge(t, edge, CMesh::SEdge::FT_FLAT);
     }
    }
    glEnd();
    glDisable(GL_BLEND);

    if(m_texFolds && m_texFolds->isBound())
        m_texFolds->release();
}

void Renderer2Dex::RenderFlap(void *tr, int edge) const
{
    const CMesh::STriangle2D& t = *static_cast<CMesh::STriangle2D*>(tr);
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];
    const glm::vec2 vN = t.GetNormal(edge) * 0.5f;

    float x[4];
    float y[4];
    if(t.IsFlapSharp(edge))
    {
        x[0] = v1.x;
        y[0] = v1.y;
        x[1] = 0.5f*v1.x + 0.5f*v2.x + vN.x;
        y[1] = 0.5f*v1.y + 0.5f*v2.y + vN.y;
        x[2] = v2.x;
        y[2] = v2.y;
        x[3] = 0.5f*v1.x + 0.5f*v2.x;
        y[3] = 0.5f*v1.y + 0.5f*v2.y;
    } else {
        x[0] = v1.x;
        y[0] = v1.y;
        x[1] = 0.9f*v1.x + 0.1f*v2.x + vN.x;
        y[1] = 0.9f*v1.y + 0.1f*v2.y + vN.y;
        x[2] = 0.1f*v1.x + 0.9f*v2.x + vN.x;
        y[2] = 0.1f*v1.y + 0.9f*v2.y + vN.y;
        x[3] = v2.x;
        y[3] = v2.y;
    }

    static const glm::mat2 rotMx90deg = glm::mat2( 0, 1,
                                                  -1, 0);
    const float normalScaler = 0.015f * CSettings::GetInstance().GetLineWidth();

    //render inner part of flap
    glTexCoord2f(0.0f, 0.8f); //white
    glVertex2f(x[0], y[0]);
    glVertex2f(x[1], y[1]);
    glVertex2f(x[2], y[2]);
    glVertex2f(x[3], y[3]);

    //render edges of flap
    glTexCoord2f(0.0f, 0.1f); //black
    for(int i=0; i<4; i++)
    {
        int i2 = (i+1)%4;
        const float& x1 = x[i];
        const float& x2 = x[i2];
        const float& y1 = y[i];
        const float& y2 = y[i2];
        const glm::vec2 eN = glm::normalize(rotMx90deg * glm::vec2(x2-x1, y2-y1)) * normalScaler;

        glVertex2f(x1 - eN.x, y1 - eN.y);
        glVertex2f(x1 + eN.x, y1 + eN.y);
        glVertex2f(x2 + eN.x, y2 + eN.y);
        glVertex2f(x2 - eN.x, y2 - eN.y);
    }
}

void Renderer2Dex::RenderEdge(void *tr, int edge, int foldType) const
{
    const CMesh::STriangle2D& t = *static_cast<CMesh::STriangle2D*>(tr);
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];
    const glm::vec2 vN = t.GetNormal(edge) * 0.015f * CSettings::GetInstance().GetLineWidth();
    const float len = t.GetEdgeLen(edge) * (float)CSettings::GetInstance().GetStippleLoop();

    float foldSelector = 1.0f;

    switch(foldType)
    {
    case CMesh::SEdge::FT_FLAT:
        foldSelector = 1.0f;
        break;
    case CMesh::SEdge::FT_VALLEY:
        foldSelector = 2.0f;
        break;
    case CMesh::SEdge::FT_MOUNTAIN:
        foldSelector = 3.0f;
        break;
    default: assert(false);
    }

    static const float oneForth = 1.0f/4.0f;

    glTexCoord2f(0.0f, oneForth * (foldSelector - 1.0f) + 0.1f);
    glVertex2f(v1.x - vN.x, v1.y - vN.y);

    glTexCoord2f(0.0f, oneForth * foldSelector - 0.1f);
    glVertex2f(v1.x + vN.x, v1.y + vN.y);

    glTexCoord2f(len, oneForth * foldSelector - 0.1f);
    glVertex2f(v2.x + vN.x, v2.y + vN.y);

    glTexCoord2f(len, oneForth * (foldSelector - 1.0f) + 0.1f);
    glVertex2f(v2.x - vN.x, v2.y - vN.y);
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
