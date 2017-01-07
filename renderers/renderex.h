#ifndef RENDERER_EX_H
#define RENDERER_EX_H

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/matrix.hpp>
#include <unordered_set>
#include <QImage>
#include <QOpenGLTexture>
#include "renderbase2d.h"

class CMesh;

class Renderer2Dex : public IRenderer2D
{
public:
    Renderer2Dex();
    virtual ~Renderer2Dex();

    void    Init() override;
    void    ResizeView(int w, int h) override;

    void    PreDraw() const override;
    void    DrawScene() const override;
    void    DrawSelection(const SSelectionInfo& sinfo) const override;
    void    DrawPaperSheet(const glm::vec2 &position, const glm::vec2 &size) const override;

    void    RecalcProjection() override;

    QImage  DrawImageFromSheet(const glm::vec2 &pos) const override;

    void    ClearTextures() override;

private:
    void    DrawParts() const;
    void    DrawFlaps() const;
    void    DrawGroups() const;
    void    DrawEdges() const;
    void    RenderFlap(void *tr, int edge) const;
    void    RenderEdge(void *tr, int edge, int foldType) const;

    void    BindTexture(unsigned id) const;
    void    UnbindTexture() const;

    mutable int m_boundTextureID = -1;

    struct NVGcontext* vg;
};

#endif // RENDERER_EX_H
