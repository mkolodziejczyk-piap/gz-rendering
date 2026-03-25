/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include <gz/common/Console.hh>

#include "gz/rendering/RenderPassSystem.hh"
#include "gz/rendering/ogre2/Ogre2GaussianNoisePass.hh"
#include "gz/rendering/ogre2/Ogre2RenderEngine.hh"

#ifdef _MSC_VER
  #pragma warning(push, 0)
#endif
#include <Compositor/OgreCompositorManager2.h>
#include <Compositor/OgreCompositorNodeDef.h>
#include <Compositor/Pass/PassQuad/OgreCompositorPassQuadDef.h>
#include <Compositor/Pass/PassClear/OgreCompositorPassClearDef.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreRoot.h>
#include <OgreTechnique.h>
#include <OgreVector3.h>

#include <OgreCamera.h>

#include <Compositor/OgreCompositorWorkspace.h>
#include <OgreSceneManager.h>


#ifdef _MSC_VER
  #pragma warning(pop)
#endif

/// \brief Private data for the Ogre2GaussianNoisePass class
class gz::rendering::Ogre2GaussianNoisePassPrivate
{
  /// brief Pointer to the Gaussian noise ogre material
  public: Ogre::Material *BlurHMat = nullptr;
  public: Ogre::Material *BlurVMat = nullptr;
  public: Ogre::Material *FogMat = nullptr;

  double projectionA;
  double projectionB;
};

using namespace gz;
using namespace rendering;

//////////////////////////////////////////////////
Ogre2GaussianNoisePass::Ogre2GaussianNoisePass()
  : dataPtr(std::make_unique<Ogre2GaussianNoisePassPrivate>())
{
}

//////////////////////////////////////////////////
Ogre2GaussianNoisePass::~Ogre2GaussianNoisePass()
{
}

//////////////////////////////////////////////////
void Ogre2GaussianNoisePass::PreRender()
// in newer versions
// void Ogre2GaussianNoisePass::PreRender(const CameraPtr &/*_camera*/)
{
  if (!this->dataPtr->FogMat)
    return;

  if (!this->enabled)
    return;

  // modify material here (wont alter the base material!), called for
  // every drawn geometry instance (i.e. compositor render_quad)

  // Sample three values within the range [0,1.0] and set them for use in
  // the fragment shader, which will interpret them as offsets from (0,0)
  // to use when computing pseudo-random values.
  Ogre::Vector3 offsets(math::Rand::DblUniform(0.0, 1.0),
                        math::Rand::DblUniform(0.0, 1.0),
                        math::Rand::DblUniform(0.0, 1.0));
  // These calls are setting parameters that are declared in two places:
  // 1. media/materials/scripts/gaussian_noise.material, in
  //    fragment_program GaussianNoiseFS
  // 2. media/materials/scripts/gaussian_noise_fs.glsl
  // Ogre::Pass *pass =
  //     this->dataPtr->gaussianNoiseMat->getTechnique(0)->getPass(0);
  // Ogre::GpuProgramParametersSharedPtr psParams =
  //     pass->getFragmentProgramParameters();
  // psParams->setNamedConstant("offsets", offsets);
  // psParams->setNamedConstant("mean", static_cast<Ogre::Real>(this->mean));
  // psParams->setNamedConstant("stddev",
  //     static_cast<Ogre::Real>(this->stdDev));
}

//////////////////////////////////////////////////
void Ogre2GaussianNoisePass::CreateRenderPass()
{
  if (!this->ogreCompositorNodeDefName.empty())
    return;

  // The GaussianNoise material is defined in script (gaussian_noise.material).
  // clone the material
  std::string matName_BlurH = "BlurH";
  std::string matName_BlurV = "BlurV";
  // std::string matName_Fog = "Fog";
  std::string matName_Fog = "GaussianNoise";

  Ogre::MaterialPtr ogreMat_BlurH =
      Ogre::MaterialManager::getSingleton().getByName(matName_BlurH);
  Ogre::MaterialPtr ogreMat_BlurV =
      Ogre::MaterialManager::getSingleton().getByName(matName_BlurV);
  Ogre::MaterialPtr ogreMat_Fog =
      Ogre::MaterialManager::getSingleton().getByName(matName_Fog);

  if (!ogreMat_BlurH)
  {
    gzerr << "BlurH material not found: '" << matName_BlurH << "'"
           << std::endl;
    return;
  }
  if (!ogreMat_BlurV)
  {
    gzerr << "BlurV material not found: '" << matName_BlurV << "'"
           << std::endl;
    return;
  }
  if (!ogreMat_Fog)
  {
    gzerr << "Fog material not found: '" << matName_Fog << "'"
           << std::endl;
    return;
  }

  if (!ogreMat_BlurH->isLoaded())
    ogreMat_BlurH->load();
  if (!ogreMat_BlurV->isLoaded())
    ogreMat_BlurV->load();
  if (!ogreMat_Fog->isLoaded())
    ogreMat_Fog->load();  

  static int gaussianNodeCounter = 0;

  std::string materialName_BlurH = matName_BlurH + "_" +
      std::to_string(gaussianNodeCounter);
  std::string materialName_BlurV = matName_BlurV + "_" +
      std::to_string(gaussianNodeCounter);
  std::string materialName_Fog = matName_Fog + "_" +
      std::to_string(gaussianNodeCounter);

  this->dataPtr->BlurHMat = ogreMat_BlurH->clone(materialName_BlurH).get();
  this->dataPtr->BlurVMat = ogreMat_BlurV->clone(materialName_BlurV).get();
  this->dataPtr->FogMat = ogreMat_Fog->clone(materialName_Fog).get();

  // create the compostior node definition

  // We need to programmatically create the compositor because we need to
  // configure it to use the cloned gaussian material created earlier.
  // The compositor workspace definition is equivalent to the following
  // ogre compositor script:
  // compositor_node GaussianNoiseNode
  // {
  //   // render texture input from previous render pass
  //   in 0 rt_input
  //   // render texture output to be passed to next render pass
  //   in 1 rt_output
  //
  //   // Only one target pass is needed.
  //   // rt_input is used as input to this pass and result is stored
  //   // in rt_output
  //   target rt_output
  //   {
  //     pass render_quad
  //     {
  //       material GaussianNoise // Use copy instead of original
  //       input 0 rt_input
  //     }
  //   }
  //   // pass the result to the next render pass
  //   out 0 rt_output
  //   // pass the rt_input render texture to the next render pass
  //   // where the texture is reused to store its result
  //   out 1 rt_input
  // }

  // this->Scene()->SensorCount();

  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();

  // const Ogre::Camera *camera =
  //     ogreRoot->_getCurrentSceneManager()->getCamerasInProgress().renderingCamera;

  // gzerr << "camera: '" << ogreRoot->_getCurrentSceneManager()->getName() << std::endl;

  // auto itor = ogreRoot->getSceneManagerIterator();
  // while (itor.hasMoreElements())
  // {
  //   Ogre::SceneManager *sceneManager = itor.getNext();
  //   gzerr << "camera: '" << sceneManager->getName() << std::endl;
  // }

  Ogre::SceneManager *sceneManager = ogreRoot->getSceneManager("SceneManagerInstance1");

  gzerr << "sceneManager: " << sceneManager->getName() << std::endl;

  const Ogre::Camera *ogreCamera = sceneManager->getCamerasInProgress().renderingCamera;

  gzerr << "camera: " << ogreCamera->getName() << std::endl;

  double farPlane = ogreCamera->getFarClipDistance();
  Ogre::Vector2 projectionAB = ogreCamera->getProjectionParamsAB();
  double projectionA = projectionAB.x;
  double projectionB = projectionAB.y;

  gzerr << "farPlane: " << farPlane << std::endl;
  gzerr << "projectionA: " << projectionA << std::endl;
  gzerr << "projectionB: " << projectionB << std::endl;

  projectionB /= farPlane;

  gzerr << "projectionB / farPlane: '" << projectionB << std::endl;  

  Ogre::CompositorManager2 *ogreCompMgr = ogreRoot->getCompositorManager2();

  std::string nodeDefName = "GaussianNoiseNodeNode_"
      + std::to_string(gaussianNodeCounter);

  this->ogreCompositorNodeDefName = nodeDefName;
  gaussianNodeCounter++;

  Ogre::CompositorNodeDef *nodeDef =
      ogreCompMgr->addNodeDefinition(nodeDefName);

  // Input texture
  nodeDef->addTextureSourceName("rt_input", 0,
      Ogre::TextureDefinitionBase::TEXTURE_INPUT);
  nodeDef->addTextureSourceName("rt_output", 1,
      Ogre::TextureDefinitionBase::TEXTURE_INPUT);

  Ogre::TextureDefinitionBase::TextureDefinition *depthTexDef =
      nodeDef->addTextureDefinition("depthTexture");
  depthTexDef->textureType = Ogre::TextureTypes::Type2D;
  depthTexDef->width = 0;
  depthTexDef->height = 0;
  depthTexDef->depthOrSlices = 1;
  depthTexDef->numMipmaps = 0;
  depthTexDef->widthFactor = 1;
  depthTexDef->heightFactor = 1;
  depthTexDef->format = Ogre::PFG_D32_FLOAT;
  depthTexDef->textureFlags &= ~Ogre::TextureFlags::Uav;
  // depthTexDef->textureFlags &= ~Ogre::TextureFlags::DiscardableContent;  
  depthTexDef->depthBufferId = 1; //Ogre::DepthBuffer::POOL_DEFAULT;
  // depthTexDef->depthBufferId = Ogre::DepthBuffer::POOL_DEFAULT;
  depthTexDef->depthBufferFormat = Ogre::PFG_UNKNOWN;
  depthTexDef->fsaa = "0";
  // keep_content

  Ogre::RenderTargetViewDef *rtvDepth =
    nodeDef->addRenderTextureView("depthTexture");
  rtvDepth->setForTextureDefinition("depthTexture", depthTexDef );
  rtvDepth->depthAttachment.textureName = "depthTexture";    
  rtvDepth->preferDepthTexture = true; 

  // Ogre::TextureDefinitionBase::TextureDefinition *rt0TexDef =
  //     nodeDef->addTextureDefinition("rt0");
  // rt0TexDef->textureType = Ogre::TextureTypes::Type2D;
  // rt0TexDef->width = 0;
  // rt0TexDef->height = 0;
  // rt0TexDef->depthOrSlices = 1;
  // rt0TexDef->numMipmaps = 0;
  // rt0TexDef->widthFactor = 1;
  // rt0TexDef->heightFactor = 1;
  // rt0TexDef->format = Ogre::PFG_RGBA8_UNORM_SRGB;
  // rt0TexDef->textureFlags &= ~Ogre::TextureFlags::Uav;
  // rt0TexDef->depthBufferId = Ogre::DepthBuffer::POOL_DEFAULT;
  // rt0TexDef->depthBufferFormat = Ogre::PFG_UNKNOWN;
  // rt0TexDef->fsaa = "0";

  Ogre::TextureDefinitionBase::TextureDefinition *rt1TexDef =
      nodeDef->addTextureDefinition("rt1");
  rt1TexDef->textureType = Ogre::TextureTypes::Type2D;
  rt1TexDef->width = 0;
  rt1TexDef->height = 0;
  rt1TexDef->depthOrSlices = 1;
  rt1TexDef->numMipmaps = 0;
  rt1TexDef->widthFactor = 1;
  rt1TexDef->heightFactor = 1;
  rt1TexDef->format = Ogre::PFG_RGBA8_UNORM_SRGB;
  // rt1TexDef->textureFlags &= ~Ogre::TextureFlags::Uav;
  // rt1TexDef->depthBufferId = Ogre::DepthBuffer::POOL_DEFAULT;
  // rt1TexDef->depthBufferFormat = Ogre::PFG_UNKNOWN;
  // rt1TexDef->fsaa = "0";

  Ogre::RenderTargetViewDef *rtvRt1 =
    nodeDef->addRenderTextureView("rt1");
  rtvRt1->setForTextureDefinition("rt1", rt1TexDef);   

  // rt_input target
  // nodeDef->setNumTargetPass(2);
  // nodeDef->setNumTargetPass(1);
  nodeDef->setNumTargetPass(3);

  Ogre::CompositorTargetDef *depthTargetDef =
      nodeDef->addTargetPass("depthTexture");
  depthTargetDef->setNumPasses(1);
  {
    // scene pass
    Ogre::CompositorPassSceneDef *passScene =
        static_cast<Ogre::CompositorPassSceneDef *>(
        depthTargetDef->addPass(Ogre::PASS_SCENE));
    passScene->setAllLoadActions(Ogre::LoadAction::Clear);
    // passScene->setAllClearColours(Ogre::ColourValue(
    //   this->FarClipPlane(),
    //   this->FarClipPlane(),
    //   this->FarClipPlane()));
    // depth texture does not contain particles
    // passScene->setVisibilityMask(
    //   GZ_VISIBILITY_ALL & ~Ogre2ParticleEmitter::kParticleVisibilityFlags);
    passScene->mEnableForwardPlus = false;
    passScene->setLightVisibilityMask(0x0);
  }

  // BlurH pass
  Ogre::CompositorTargetDef *blurHTargetDef =
        nodeDef->addTargetPass("rt1");
  blurHTargetDef->setNumPasses(1);
  {
    // quad pass
    Ogre::CompositorPassQuadDef *passQuad =
        static_cast<Ogre::CompositorPassQuadDef *>(
        blurHTargetDef->addPass(Ogre::PASS_QUAD));
    passQuad->mMaterialName = materialName_BlurH;
    passQuad->addQuadTextureSource(0, "rt_input");
  }

  // Fog pass
  Ogre::CompositorTargetDef *inputTargetDef =
      nodeDef->addTargetPass("rt_output");
  inputTargetDef->setNumPasses(1);
  {
    // quad pass
    Ogre::CompositorPassQuadDef *passQuad =
        static_cast<Ogre::CompositorPassQuadDef *>(
        inputTargetDef->addPass(Ogre::PASS_QUAD));
    passQuad->setAllLoadActions(Ogre::LoadAction::Clear);    
    passQuad->mMaterialName = materialName_Fog;
    passQuad->addQuadTextureSource(0, "rt_input");
    // passQuad->addQuadTextureSource(1, "depthTexture");
    passQuad->addQuadTextureSource(1, "rt1");
    passQuad->addQuadTextureSource(2, "depthTexture");
    passQuad->mFrustumCorners =
          Ogre::CompositorPassQuadDef::VIEW_SPACE_CORNERS;
  }
  nodeDef->mapOutputChannel(0, "rt_output");
  nodeDef->mapOutputChannel(1, "rt_input");
}

GZ_RENDERING_REGISTER_RENDER_PASS(Ogre2GaussianNoisePass, GaussianNoisePass)
