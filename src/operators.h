/* Copyright 2013-2016 Matt Tytel
 *
 * mopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mopo.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef OPERATORS_H
#define OPERATORS_H

#include "magnitude_lookup.h"
#include "midi_lookup.h"
#include "resonance_lookup.h"
#include "processor.h"

#define PROCESS_TICK_FUNCTION \
void process() override { \
  for (int i = 0; i < buffer_size_; ++i) \
    tick(i); \
  processTriggers(); \
}

namespace mopo {

  // A base class for arithmetic operators.
  class Operator : public Processor {
    public:
      Operator(int num_inputs, int num_outputs) :
          Processor(num_inputs, num_outputs) { }

      virtual void process() override;
      virtual void tick(int i) = 0;
      inline void processTriggers() {
        output()->clearTrigger();
        for (int i = 0; i < numInputs(); ++i) {
          if (input(i)->source->triggered) {
            int offset = input(i)->source->trigger_offset;
            tick(offset);
            output()->trigger(output()->buffer[offset], offset);
          }
        }
      }

    private:
      Operator() : Processor(0, 0) { }
  };

  // A processor that will clamp a signal output to a given window.
  class Clamp : public Operator {
    public:
      Clamp(mopo_float min = -1, mopo_float max = 1) : Operator(1, 1),
                                                       min_(min), max_(max) { }

      virtual Processor* clone() const override { return new Clamp(*this); }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = utils::clamp(source[i], min_, max_);
      }

    private:
      mopo_float min_, max_;
  };

  // A processor that passes input to output.
  class Bypass : public Operator {
    public:
      Bypass() : Operator(1, 1) { }

      virtual Processor* clone() const override { return new Bypass(*this); }

      void process() override {
        memcpy(output()->buffer, input()->source->buffer,
               buffer_size_ * sizeof(mopo_float));

        output()->triggered = input()->source->triggered;
        output()->trigger_value = input()->source->trigger_value;
        output()->trigger_offset = input()->source->trigger_offset;
      }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = source[i];
      }
  };

  // A processor that will negate the amplitude of a signal.
  class Negate : public Operator {
    public:
      Negate() : Operator(1, 1) { }

      virtual Processor* clone() const override { return new Negate(*this); }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = -source[i];
      }
  };

  // A processor that will invert a signal value by value x -> x^-1.
  class Inverse : public Operator {
    public:
      Inverse() : Operator(1, 1) { }

      virtual Processor* clone() const override { return new Inverse(*this); }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = 1.0 / source[i];
      }

      PROCESS_TICK_FUNCTION
  };

  // A processor that will scale a signal by a given scalar.
  class LinearScale : public Operator {
    public:
      LinearScale(mopo_float scale = 1) : Operator(1, 1), scale_(scale) { }
      virtual Processor* clone() const override {
        return new LinearScale(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = scale_ * source[i];
      }

    private:
      mopo_float scale_;
  };

  // A processor that will raise a signal to a given power.
  class Square : public Operator {
    public:
      Square() : Operator(1, 1) { }
      virtual Processor* clone() const override { return new Square(*this); }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = source[i] * source[i];
      }

      PROCESS_TICK_FUNCTION
  };

  // A processor that will raise a given number to the power of a signal.
  class ExponentialScale : public Operator {
    public:
      ExponentialScale(mopo_float scale = 1) :
          Operator(1, 1), scale_(scale) { }

      virtual Processor* clone() const override {
        return new ExponentialScale(*this);
      }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = std::pow(scale_, source[i]);
      }

      PROCESS_TICK_FUNCTION

    private:
      mopo_float scale_;
  };

  // A processor that will convert a stream of midi to a stream of frequencies.
  class MidiScale : public Operator {
    public:
      MidiScale() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new MidiScale(*this);
      }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = MidiLookup::centsLookup(CENTS_PER_NOTE * source[i]);
      }

      PROCESS_TICK_FUNCTION
  };

  // A processor that will convert a stream of magnitudes to a stream of
  // q resonance values.
  class ResonanceScale : public Operator {
    public:
      ResonanceScale() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new ResonanceScale(*this);
      }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = ResonanceLookup::qLookup(source[i]);
      }

      PROCESS_TICK_FUNCTION
  };

  // A processor that will convert a stream of decibals to a stream of
  // magnitudes.
  class MagnitudeScale : public Operator {
    public:
      MagnitudeScale() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new MagnitudeScale(*this);
      }

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = MagnitudeLookup::magnitudeLookup(source[i]);
      }

      PROCESS_TICK_FUNCTION
  };

  // A processor that will add two streams together.
  class Add : public Operator {
    public:
      Add() : Operator(2, 1) { }

      virtual Processor* clone() const override { return new Add(*this); }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input(0)->source->buffer,
                                     input(1)->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest,
                             const mopo_float* source_left,
                             const mopo_float* source_right, int i) {
        dest[i] = source_left[i] + source_right[i];
      }
  };

  // A processor that will add a variable number of strings together.
  class VariableAdd : public Operator {
    public:
      VariableAdd(int num_inputs = 0) : Operator(num_inputs, 1) { }

      virtual Processor* clone() const override {
        return new VariableAdd(*this);
      }

      void process() override;

      inline void tick(int i) override {
        int num_inputs = inputs_->size();
        output()->buffer[i] = 0.0;
        for (int in = 0; in < num_inputs; ++in)
          output()->buffer[i] += input(in)->at(i);
      }
  };

  // A processor that will subtract one stream from another.
  class Subtract : public Operator {
    public:
      Subtract() : Operator(2, 1) { }

      virtual Processor* clone() const override { return new Subtract(*this); }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input(0)->source->buffer,
                                     input(1)->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest,
                             const mopo_float* source_left,
                             const mopo_float* source_right, int i) {
        dest[i] = source_left[i] - source_right[i];
      }
  };

  // A processor that will multiply to streams together.
  class Multiply : public Operator {
    public:
      Multiply() : Operator(2, 1) { }

      virtual Processor* clone() const override { return new Multiply(*this); }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input(0)->source->buffer,
                                     input(1)->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest,
                             const mopo_float* source_left,
                             const mopo_float* source_right, int i) {
        dest[i] = source_left[i] * source_right[i];
      }
  };

  // A processor that will interpolate two streams by an interpolation stream.
  class Interpolate : public Operator {
    public:
      enum Inputs {
        kFrom,
        kTo,
        kFractional,
        kNumInputs
      };

      Interpolate() : Operator(kNumInputs, 1) { }

      virtual Processor* clone() const override {
        return new Interpolate(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input(kFrom)->source->buffer,
                                     input(kTo)->source->buffer,
                                     input(kFractional)->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest,
                             const mopo_float* source_left,
                             const mopo_float* source_right,
                             const mopo_float* fraction, int i) {
        dest[i] = INTERPOLATE(source_left[i], source_right[i], fraction[i]);
      }
  };

  // A processor that will interpolate two streams by an interpolation stream.
  class BilinearInterpolate : public Operator {
    public:
      enum Inputs {
        kTopLeft,
        kTopRight,
        kBottomLeft,
        kBottomRight,
        kXPosition,
        kYPosition,
        kNumInputs
      };

      BilinearInterpolate() : Operator(kNumInputs, 1) { }

      virtual Processor* clone() const override {
        return new BilinearInterpolate(*this);
      }

      void process() override;

      inline void tick(int i) override {
        float top = INTERPOLATE(input(kTopLeft)->at(i),
                                input(kTopRight)->at(i),
                                input(kXPosition)->at(i));
        float bottom = INTERPOLATE(input(kBottomLeft)->at(i),
                                   input(kBottomRight)->at(i),
                                   input(kXPosition)->at(i));
        output()->buffer[i] = INTERPOLATE(top, bottom,
                                          input(kYPosition)->at(i));
      }
  };

  class FrequencyToPhase : public Operator {
    public:
      FrequencyToPhase() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new FrequencyToPhase(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = source[i] / sample_rate_;
      }
  };

  class FrequencyToSamples : public Operator {
    public:
      FrequencyToSamples() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new FrequencyToSamples(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = sample_rate_ / source[i];
      }
  };

  class TimeToSamples : public Operator {
    public:
      TimeToSamples() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new TimeToSamples(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = sample_rate_ * source[i];
      }
  };

  class SampleAndHoldBuffer : public Operator {
    public:
      SampleAndHoldBuffer() : Operator(1, 1) { }

      virtual Processor* clone() const override {
        return new SampleAndHoldBuffer(*this);
      }

      void process() override;

      inline void tick(int i) override {
        bufferTick(output()->buffer, input()->source->buffer, i);
      }

      inline void bufferTick(mopo_float* dest, const mopo_float* source, int i) {
        dest[i] = source[0];
      }
  };

  class LinearSmoothBuffer : public Operator {
    public:
      enum Inputs {
        kValue,
        kTrigger,
        kNumInputs
      };

      LinearSmoothBuffer() : Operator(kNumInputs, 1), last_value_(1.0) { }

      virtual Processor* clone() const override {
        return new LinearSmoothBuffer(*this);
      }

      void process() override;

      inline void tick(int i) override {
        output()->buffer[i] = input()->at(0);
      }

    protected:
      mopo_float last_value_;
  };

  namespace cr {

    class Add : public Operator {
      public:
        Add() : Operator(2, 1) {
          setControlRate(true);
        }

        virtual Processor* clone() const override { return new Add(*this); }

        inline void tick(int i) override {
          output()->buffer[0] = input(0)->at(0) + input(1)->at(0);
        }

        void process() override {
          tick(0);
        }
    };

    class Multiply : public Operator {
      public:
        Multiply() : Operator(2, 1) {
          setControlRate(true);
        }

        virtual Processor* clone() const override { return new Multiply(*this); }

        inline void tick(int i) override {
          output()->buffer[0] = input(0)->at(0) * input(1)->at(0);
        }

        void process() override {
          tick(0);
        }
    };

    class VariableAdd : public Operator {
      public:
        VariableAdd(int num_inputs = 0) : Operator(num_inputs, 1) {
          setControlRate(true);
        }

        virtual Processor* clone() const override {
          return new VariableAdd(*this);
        }

        void process() override {
          tick(0);
        }

        inline void tick(int i) override {
          size_t num_inputs = inputs_->size();
          output()->buffer[0] = 0.0;

          for (int in = 0; in < num_inputs; ++in)
            output()->buffer[0] += input(in)->at(0);
        }
    };
  } // namespace cr
} // namespace mopo

#endif // OPERATORS_H
