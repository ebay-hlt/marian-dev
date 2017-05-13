#pragma once

#include "marian.h"

namespace marian {
  
class ScorerState {
  public:
    virtual Expr getProbs() = 0;
    
    virtual float breakDown(size_t i) {
      return getProbs()->val()->get(i);
    }
};

class Scorer {
  protected:
    std::string name_;
    float weight_;
  
  public:
    Scorer(const std::string& name, float weight)
    : name_(name), weight_(weight) {}
    
    std::string getName() { return name_; }
    float getWeight() { return weight_; }
    
    virtual void clear(Ptr<ExpressionGraph>) = 0;
    virtual Ptr<ScorerState> startState(Ptr<ExpressionGraph>, Ptr<data::CorpusBatch>) = 0;
    virtual Ptr<ScorerState> step(Ptr<ExpressionGraph>,
                                  Ptr<ScorerState>,
                                  const std::vector<size_t>&,
                                  const std::vector<size_t>&) = 0;
    
    virtual void init(Ptr<ExpressionGraph> graph) {}
};

class ScorerWrapperState : public ScorerState {
  protected:
    Ptr<DecoderState> state_;
  
  public:
    ScorerWrapperState(Ptr<DecoderState> state) : state_(state) {}
    
    virtual Ptr<DecoderState> getState() {
      return state_;
    }
    
    virtual Expr getProbs() {
      return state_->getProbs();
    };
};


template <class EncoderDecoder>
class ScorerWrapper : public Scorer {
  private:
    Ptr<EncoderDecoder> encdec_;
    std::string fname_;
  
  public:
    template <class ...Args>
    ScorerWrapper(const std::string& name, float weight,
                  const std::string& fname,
                  Ptr<Config> options, Args ...args)
    : Scorer(name, weight),
      encdec_(New<EncoderDecoder>(options, std::vector<size_t>({0, 1}),
                                  keywords::inference=true, args...)),
      fname_(fname)
    {}

    template <class ...Args>
    ScorerWrapper(const std::string& name, float weight,
                  const std::string& fname,
                  Ptr<Config> options,
                  const std::vector<size_t>& batchIndices,
                  Args ...args)
    : Scorer(name, weight),
      encdec_(New<EncoderDecoder>(options, batchIndices,
                                  keywords::inference=true, args...)),
      fname_(fname)
    {}
    
    virtual void init(Ptr<ExpressionGraph> graph) {
      graph->switchParams(getName());
      encdec_->load(graph, fname_);
    }
    
    virtual void clear(Ptr<ExpressionGraph> graph) {
      graph->switchParams(getName());
      encdec_->clear(graph);
    }
    
    virtual Ptr<ScorerState> startState(Ptr<ExpressionGraph> graph,
                                        Ptr<data::CorpusBatch> batch) {
      graph->switchParams(getName());
      return New<ScorerWrapperState>(encdec_->startState(graph, batch));
    }
    
    virtual Ptr<ScorerState> step(Ptr<ExpressionGraph> graph,
                                  Ptr<ScorerState> state,
                                  const std::vector<size_t>& hypIndices,
                                  const std::vector<size_t>& embIndices) {
      graph->switchParams(getName());
      auto wrappedState = std::dynamic_pointer_cast<ScorerWrapperState>(state)->getState();
      return New<ScorerWrapperState>(encdec_->step(graph, wrappedState, hypIndices, embIndices));
    }  
};

class WordPenaltyState : public ScorerState {
  private:
    int dimVocab_;
    Expr penalties_;
  
  public:
    WordPenaltyState(int dimVocab, Expr penalties)
    : dimVocab_(dimVocab), penalties_(penalties)
    {}
    
    virtual Expr getProbs() { return penalties_; };
    
    virtual float breakDown(size_t i) {
      return getProbs()->val()->get(i % dimVocab_);
    }
};

class WordPenalty : public Scorer {
  private:
    int dimVocab_;
    Expr penalties_;
  
  public:
    WordPenalty(const std::string& name, float weight, int dimVocab)
    : Scorer(name, weight), dimVocab_(dimVocab) {}
    
    virtual void clear(Ptr<ExpressionGraph> graph) {}
    
    virtual Ptr<ScorerState> startState(Ptr<ExpressionGraph> graph,
                                        Ptr<data::CorpusBatch> batch) {
      std::vector<float> p(dimVocab_, 1);
      p[0] = 0;
      p[2] = 0;
      
      penalties_ = graph->constant(keywords::shape={1, dimVocab_},
                                   keywords::init=inits::from_vector(p));
      return New<WordPenaltyState>(dimVocab_, penalties_);
    }
    
    virtual Ptr<ScorerState> step(Ptr<ExpressionGraph> graph,
                                  Ptr<ScorerState> state,
                                  const std::vector<size_t>& hypIndices,
                                  const std::vector<size_t>& embIndices) {
      return state;
    }
};

class UnseenWordPenalty : public Scorer {
  private:
    int batchIndex_;
    int dimVocab_;
    Expr penalties_;
  
  public:
    UnseenWordPenalty(const std::string& name, float weight, int dimVocab, int batchIndex)
    : Scorer(name, weight), dimVocab_(dimVocab), batchIndex_(batchIndex) {}
    
    virtual void clear(Ptr<ExpressionGraph> graph) {}
    
    virtual Ptr<ScorerState> startState(Ptr<ExpressionGraph> graph,
                                        Ptr<data::CorpusBatch> batch) {
      std::vector<float> p(dimVocab_, -1);
      for(auto i : (*batch)[batchIndex_]->indeces())
        p[i] = 0;
      p[2] = 0;
      
      penalties_ = graph->constant(keywords::shape={1, dimVocab_},
                                   keywords::init=inits::from_vector(p));
      return New<WordPenaltyState>(dimVocab_, penalties_);
    }
    
    virtual Ptr<ScorerState> step(Ptr<ExpressionGraph> graph,
                                  Ptr<ScorerState> state,
                                  const std::vector<size_t>& hypIndices,
                                  const std::vector<size_t>& embIndices) {
      return state;
    }
};

}