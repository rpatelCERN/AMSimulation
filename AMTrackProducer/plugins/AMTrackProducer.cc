#ifndef NTupleTools_AMTrackProducer_h_

#include <memory>
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "FWCore/Framework/interface/MakerMacros.h"
#include "DataFormats/GeometryCommonDetAlgo/interface/MeasurementPoint.h"
#include "DataFormats/L1TrackTrigger/interface/TTCluster.h"
#include "DataFormats/L1TrackTrigger/interface/TTStub.h"
#include "DataFormats/L1TrackTrigger/interface/TTTrack.h"
#include "DataFormats/L1TrackTrigger/interface/TTTypes.h"

#include "Geometry/Records/interface/StackedTrackerGeometryRecord.h"
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "Geometry/TrackerGeometryBuilder/interface/StackedTrackerGeometry.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "AMCMSSWInterface/AMTrackProducer/interface/LinearizedTrackFitter.h"
#include<map>

class AMTrackProducer : public edm::EDProducer
{
public:
  explicit AMTrackProducer(const edm::ParameterSet&);

private:
  virtual void beginJob();
  virtual void produce(edm::Event&, const edm::EventSetup&);
  virtual void endJob();

  edm::InputTag RoadsTag_;
  edm::InputTag StubsTag_;
  std::shared_ptr<LinearizedTrackFitter> linearizedTrackFitter_;
  std::string constantsDir_;
};


void AMTrackProducer::beginJob()
{
  linearizedTrackFitter_ = (std::make_shared<LinearizedTrackFitter>("/fdata/hepx/store/user/rish/AMSIMULATION/Forked/CMSSW_6_2_0_SLHC25_patch3/src/LinearizedTrackFit/LinearizedTrackFit/python/ConstantsProduction/",
								    true, true));
}


void AMTrackProducer::endJob() {}


AMTrackProducer::AMTrackProducer(const edm::ParameterSet& iConfig)
{
  StubsTag_ =(iConfig.getParameter<edm::InputTag>("inputTagStub"));
  RoadsTag_=(iConfig.getParameter<edm::InputTag>("RoadsInputTag"));
  produces< std::vector< TTTrack< Ref_PixelDigi_ > > >( "Level1TTTracks" ).setBranchAlias("Level1TTTracks");
  constantsDir_= iConfig.getParameter<std::string>("ConstantsDir");
  //edm::FileInPath fp = iConfig.getParameter<edm::FileInPath>("ConstantsDir");
  //constantsDir_ = fp.fullPath();
}


void AMTrackProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  std::auto_ptr< std::vector< TTTrack< Ref_PixelDigi_ > > > L1TkTracksForOutput( new std::vector< TTTrack< Ref_PixelDigi_ > > );
  edm::Handle<edmNew::DetSetVector<TTStub<Ref_PixelDigi_> > > pixelDigiTTStubs;
  iEvent.getByLabel(StubsTag_, pixelDigiTTStubs);

  std::auto_ptr<std::vector< TTTrack<Ref_PixelDigi_> > > TTTrackVector(new std::vector<TTTrack<Ref_PixelDigi_> >());

  edm::Handle< std::vector< TTTrack< Ref_PixelDigi_ > > > TTRoadHandle;
  iEvent.getByLabel( RoadsTag_, TTRoadHandle );
  edm::ESHandle<StackedTrackerGeometry> stackedGeometryHandle;
  iSetup.get<StackedTrackerGeometryRecord>().get(stackedGeometryHandle);
  const StackedTrackerGeometry *theStackedGeometry = stackedGeometryHandle.product();

  if (TTRoadHandle->size() > 0 ){
    unsigned int tkCnt = 0;
    std::vector< TTTrack< Ref_PixelDigi_ > >::const_iterator iterTTTrack;
    for (iterTTTrack = TTRoadHandle->begin(); iterTTTrack != TTRoadHandle->end(); ++iterTTTrack) {
      edm::Ptr< TTTrack< Ref_PixelDigi_ > > tempTrackPtr( TTRoadHandle, tkCnt++ );
      std::vector< edm::Ref< edmNew::DetSetVector< TTStub< Ref_PixelDigi_ > >, TTStub< Ref_PixelDigi_ > > > trackStubs = tempTrackPtr->getStubRefs();
      std::vector<double> vars;
      for(unsigned int i=0;i<trackStubs.size();i++) {
	edm::Ref< edmNew::DetSetVector< TTStub< Ref_PixelDigi_ > >, TTStub< Ref_PixelDigi_ > > tempStubRef = trackStubs.at(i);
	GlobalPoint posStub = theStackedGeometry->findGlobalPosition( &(*tempStubRef) );
	vars.push_back(posStub.phi());
	vars.push_back(posStub.perp());
	vars.push_back(posStub.z());
      }
      // Fit for this road
      unsigned comb=trackStubs.size();
      if (comb>6) comb = 6;
      double normChi2 = linearizedTrackFitter_->fit(vars, comb);	
      const std::vector<double>& pars = linearizedTrackFitter_->estimatedPars();
      float pt=1.0/fabs(pars[0]);
      float px=pt*cos(fabs(pars[1]));
      float py=pt*sin(fabs(pars[1]));
      float pz=pt*pars[2];
      GlobalVector p3(px,py,pz);
      TTTrack<Ref_PixelDigi_> aTrack;
      aTrack.setMomentum(p3,4);
      aTrack.setRInv(0.003 * 3.8114 * pars[0], 4);
      aTrack.setChi2(normChi2, 4);
      GlobalPoint POCA(0, 0, pars[3]);
      aTrack.setPOCA(POCA, 4);
      //if(ttracks[t].chi2Red()<5 && aTrack.getStubRefs().size()>=4 && !ttracks[t].isGhost())
      L1TkTracksForOutput->push_back(aTrack);
    }
    std::cout<<"Tracks "<<L1TkTracksForOutput->size()<<std::endl;
    iEvent.put( L1TkTracksForOutput, "Level1TTTracks");
  }
}


DEFINE_FWK_MODULE(AMTrackProducer);

#endif
