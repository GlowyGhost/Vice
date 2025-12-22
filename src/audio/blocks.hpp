#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <list>
#include <deque>

class Block {
public:
    virtual ~Block() = default;

    virtual float Render(float* buffer) {return *buffer;}
};

class DelayBlock : public Block {
public:
    int time_ms;
    int sample_rate;
    int delay_samples;
    std::deque<float> buffer;

    DelayBlock(int t, int sr) : time_ms(t), sample_rate(sr), delay_samples(t * sr / 1000) {}

    float Render(float* buffer) override {
        this->buffer.push_back(*buffer);
        if (this->buffer.size() > delay_samples) {
            float out = this->buffer.front();
            this->buffer.pop_front();
            return out;
        } else {
            return 0.0f;
        }
    }
};

class DistortionBlock : public Block {
public:
    int intensity;

    DistortionBlock(int i) : intensity(i) {}

    float Render(float* buffer) override {
        return *buffer;
    }
};

class CompressionBlock : public Block {
public:
    int amount;

    CompressionBlock(int a) : amount(a) {}

    float Render(float* buffer) override {
        return *buffer;
    }
};

class GainBlock : public Block {
public:
    double amount;

    GainBlock(double t) : amount(t) {}

    float Render(float* buffer) override {
        float sample = *buffer * amount;

        sample = std::max(-1.0f, std::min(1.0f, sample));
        return sample;
    }
};

class GatingBlock : public Block {
public:
    int threshold;

    GatingBlock(int t) : threshold(t) {}

    float Render(float* buffer) override {
        return *buffer;
    }
};

class ReverbBlock : public Block {
public:
    int intensity;
    int sample_rate;
    int delay_samples;
    double feedback;
    std::deque<float> buffer;

    ReverbBlock(int i, int sr) : intensity(i), sample_rate(sr), delay_samples(i * sr / 1000), feedback(0.4) {}

    float Render(float* buffer) override {
        float input = *buffer;

        float delayed = 0.0f;

        if (this->buffer.size() >= delay_samples) {
            delayed = this->buffer.front();
            this->buffer.pop_front();
        }

        float feedback_sample = input + delayed * feedback;
        this->buffer.push_back(feedback_sample);

        float output = input + delayed * 0.5f;
        return output;
    }
};

class BlocksManager {
public:
    void Initialize(const std::string& text, int sample_rate) {
        this->sample_rate = sample_rate;
        blocks.clear();

        std::istringstream input(text);
        std::string line;

        while (std::getline(input, line)) {
            if (!line.empty()) {
                blocks.push_back(CreateBlockFromLine(line));
            }
        }
    }

    float Render(float* buffer) {
        float current_buffer = *buffer;

        for (size_t i = 0; i < blocks.size(); ++i) {
            current_buffer = blocks.at(i).get()->Render(&current_buffer);
        }

        return current_buffer;
    }

private:
    int sample_rate;
    std::vector<std::unique_ptr<Block>> blocks;

    std::unique_ptr<Block> CreateBlockFromLine(const std::string& line) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        std::unordered_map<std::string, int> params;
        std::string token;

        while (iss >> token) {
            auto pos = token.find('=');
            if (pos != std::string::npos) {
                params[token.substr(0, pos)] =
                    std::stoi(token.substr(pos + 1));
            }
        }

        if (type == "delay")
            return std::make_unique<DelayBlock>(params.at("time"), sample_rate);
        if (type == "distortion")
            return std::make_unique<DistortionBlock>(params.at("intensity"));
        if (type == "compression")
            return std::make_unique<CompressionBlock>(params.at("amount"));
        if (type == "gating")
            return std::make_unique<GatingBlock>(params.at("threshold"));
        if (type == "reverb")
            return std::make_unique<ReverbBlock>(params.at("intensity"), sample_rate);
        if (type == "gain")
            return std::make_unique<GainBlock>(params.at("amount"));

        return std::make_unique<DelayBlock>(0, sample_rate);
    }
};