#!/bin/bash

# Default values for testing, can be overridden by arguments
# Example usage: ./determine_runners.sh '["self-hosted"]' '["ubuntu-latest"]'
self_hosted_labels=${1:-'["self-hosted"]'}
github_labels=${2:-'["ubuntu-latest"]'}

echo "Checking for runners with labels: $self_hosted_labels"
echo "Fallback GitHub labels: $github_labels"

# Fetch runners using gh cli
echo "Fetching runners from GitHub API..."
# Note: Assuming 'gh' is authenticated as per user instructions
runners=$(gh api -H "Accept: application/vnd.github+json" /orgs/ducktype-org/actions/runners 2>/dev/null || echo '{"runners":[]}')

# Debug print: Show the raw response from GitHub API
echo "--------------------------------"
echo "Raw Runners API Response:"
# Pretty print if possible, otherwise raw
echo "$runners" | jq '.' 2>/dev/null || echo "$runners"
echo "--------------------------------"

available_runners_json="["
first=true
used_runner_ids=()

# Get length of the array
len=$(echo "$self_hosted_labels" | jq length)

for i in $(seq 0 $(($len - 1))); do
    label=$(echo "$self_hosted_labels" | jq -r ".[$i]")
    github_label=$(echo "$github_labels" | jq -r ".[$i]")

    echo "Processing index $i: Label='$label', Fallback='$github_label'"

    # Find the first available runner with the label that hasn't been used yet
    runner_info=$(echo "$runners" | jq -c --arg lbl "$label" '
        .runners[]? |
        select(
        .status == "online" and
        .busy == false and
        (.labels[]?.name == $lbl)
        )')

    echo "  Candidates found for '$label':"
    if [ -z "$runner_info" ]; then
        echo "    None"
    else
        echo "$runner_info" | jq '.'
    fi

    selected_runner_id=""
    selected_runner_name=""
    if [ -n "$runner_info" ]; then
        while read -r runner; do
        runner_id=$(echo "$runner" | jq -r '.id')
        runner_name=$(echo "$runner" | jq -r '.name')
        
        # Check if this runner_id is already used
        skip=false
        for used_id in "${used_runner_ids[@]}"; do
            if [ "$runner_id" = "$used_id" ]; then
            skip=true
            echo "    Skipping runner $runner_name ($runner_id) - already used"
            break
            fi
        done
        if [ "$skip" = false ]; then
            selected_runner_id="$runner_id"
            selected_runner_name="$runner_name"
            echo "    Selected runner: $selected_runner_name ($selected_runner_id)"
            break
        fi
        done < <(echo "$runner_info")
    fi

    if [ -n "$selected_runner_id" ]; then
        used_runner_ids+=("$selected_runner_id")
        echo "  -> Using self-hosted runner: $selected_runner_name with ID: $selected_runner_id for label: $label"
        available_runners_json+="{\"group\":\"SelfHostedRunners\",\"labels\":\"$label\"}"
    else
        echo "  -> No available self-hosted runner found for label: $label, using GitHub label: $github_label"
        available_runners_json+="{\"labels\":\"$github_label\"}"
    fi

    if [ $i -lt $(($len - 1)) ]; then
        available_runners_json+=",";
    fi
done

available_runners_json+="]"
echo "--------------------------------"
echo "Final Result:"
echo "Available runners: ${available_runners_json}"
