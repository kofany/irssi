#!/bin/bash

# Lista SHA dangling commitów
commits=(
000e3a55b157b2f23db1c858adec34779d9b8a6b
672b06e3ffac10663e79c4026e92a2a5629fa48b
ae481ea8e11d3319b2a0e9d5b4c31cf505c6f849
a1794ac64b5e62731351a5486035fea47bbeaf63
aea05ab379c8df0af5541cbecf382eccd82a5c38
f9a9beb267d0fd1b49b0236dbc8d8b9aab0685d0
f1c6b0d9ccb511ca0bc7f6be343357701d1f6d80
60fe0c3a018c85d14e4a554482a5a313d74d54fe
0b07d909d7d3430effc33782dfe8b284628b7d02
08098111a1933b9537a3be784a4b88b7ecd7024d
a7123d8f78f57865fe15dc5dc246240195a981ef
746407c28d7d1f38a134e9ea9906a1b2d31db239
77c42d8ba9fa94749fe159933c79a32402c1a63b
63dd2d8aec0e7a6908937e0c7d6f6c12dc8c61a3
14ed5da464c55dc57635bdc71ea0f8b7df845a08
)

echo "SHA                                 | Data                | Autor             | Opis"
echo "----------------------------------------------------------------------------------------"
for sha in "${commits[@]}"; do
    git show -s --format="%h | %ci | %an | %s" $sha
done
